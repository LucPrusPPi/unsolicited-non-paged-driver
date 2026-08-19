#include "unpd/memory/universal_memory.hpp"
#include "unpd/kernel_asm.hpp"
#include "unpd/mmu/paging_engine.hpp"

#ifdef _KERNEL_MODE

// ============================================================================
// Global Kernel Operator New / Delete Implementations (Non-Aligned & Aligned)
// ============================================================================

void* __cdecl operator new(size_t size) {
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'WENK');
}

void* __cdecl operator new[](size_t size) {
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'WENK');
}

void __cdecl operator delete(void* ptr) noexcept {
    if (ptr) ExFreePoolWithTag(ptr, 'WENK');
}

void __cdecl operator delete[](void* ptr) noexcept {
    if (ptr) ExFreePoolWithTag(ptr, 'WENK');
}

void __cdecl operator delete(void* ptr, size_t) noexcept {
    if (ptr) ExFreePoolWithTag(ptr, 'WENK');
}

void __cdecl operator delete[](void* ptr, size_t) noexcept {
    if (ptr) ExFreePoolWithTag(ptr, 'WENK');
}

namespace std {
    enum class align_val_t : size_t {};
}

void* __cdecl operator new(size_t size, std::align_val_t) {
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'WENK');
}

void* __cdecl operator new[](size_t size, std::align_val_t) {
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'WENK');
}

void __cdecl operator delete(void* ptr, std::align_val_t) noexcept {
    if (ptr) ExFreePoolWithTag(ptr, 'WENK');
}

void __cdecl operator delete[](void* ptr, std::align_val_t) noexcept {
    if (ptr) ExFreePoolWithTag(ptr, 'WENK');
}

void __cdecl operator delete(void* ptr, size_t, std::align_val_t) noexcept {
    if (ptr) ExFreePoolWithTag(ptr, 'WENK');
}

void __cdecl operator delete[](void* ptr, size_t, std::align_val_t) noexcept {
    if (ptr) ExFreePoolWithTag(ptr, 'WENK');
}

namespace unpd::memory {

static constexpr ULONG SLAB_SIZES[4] = { 64, 256, 1024, 4096 };
static constexpr ULONG SLAB_TAGS[4]  = { '1LSU', '2LSU', '3LSU', '4LSU' };

// ============================================================================
// MdlMemoryEngine Implementation (Dynamic Sessions & Context Invariance)
// ============================================================================

MdlMemoryEngine::MdlMemoryEngine() noexcept
    : m_initialized(false), m_nextSessionId(1) {
    KeInitializeSpinLock(&m_lock);
    InitializeListHead(&m_sessionListHead);
}

MdlMemoryEngine::~MdlMemoryEngine() noexcept {
    Shutdown();
}

NTSTATUS MdlMemoryEngine::Initialize() noexcept {
    m_initialized = true;
    return STATUS_SUCCESS;
}

void MdlMemoryEngine::Shutdown() noexcept {
    if (!m_initialized) return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    while (!IsListEmpty(&m_sessionListHead)) {
        PLIST_ENTRY entry = RemoveHeadList(&m_sessionListHead);
        KeReleaseSpinLock(&m_lock, oldIrql);

        auto* node = CONTAINING_RECORD(entry, SessionListNode, ListEntry);
        SharedSessionDescriptor& desc = node->Descriptor;

        if (desc.UserVa && desc.Mdl) {
            PEPROCESS currentProcess = PsGetCurrentProcess();
            if (desc.OwningProcess && desc.OwningProcess != currentProcess) {
                unpd::mmu::ProcessAttachmentGuard guard(desc.OwningProcess);
                MmUnmapLockedPages(desc.UserVa, desc.Mdl);
            } else {
                MmUnmapLockedPages(desc.UserVa, desc.Mdl);
            }
            desc.UserVa = NULL;
        }

        if (desc.Mdl) {
            MmFreePagesFromMdl(desc.Mdl);
            IoFreeMdl(desc.Mdl);
            desc.Mdl = NULL;
        }

        if (desc.OwningProcess) {
            ObDereferenceObject(desc.OwningProcess);
            desc.OwningProcess = NULL;
        }

        ExFreePoolWithTag(node, 'NDPU');

        KeAcquireSpinLock(&m_lock, &oldIrql);
    }

    KeReleaseSpinLock(&m_lock, oldIrql);
    m_initialized = false;
}

kstd::expected<SharedSessionDescriptor> MdlMemoryEngine::AllocateSharedSession(ULONG pageCount) noexcept {
    if (pageCount == 0 || pageCount > 256) {
        return kstd::expected<SharedSessionDescriptor>::error(STATUS_INVALID_PARAMETER);
    }

    SIZE_T totalBytes = static_cast<SIZE_T>(pageCount) * PAGE_SIZE;
    PHYSICAL_ADDRESS lowAddr{};
    PHYSICAL_ADDRESS highAddr{};
    PHYSICAL_ADDRESS skipBytes{};
    highAddr.QuadPart = MAXULONG64;

    PMDL mdl = MmAllocatePagesForMdlEx(
        lowAddr,
        highAddr,
        skipBytes,
        totalBytes,
        MmCached,
        MM_ALLOCATE_PREFER_CONTIGUOUS
    );

    if (!mdl) {
        return kstd::expected<SharedSessionDescriptor>::error(STATUS_INSUFFICIENT_RESOURCES);
    }

    PVOID userVa = NULL;
    __try {
        userVa = MmMapLockedPagesSpecifyCache(
            mdl,
            UserMode,
            MmCached,
            NULL,
            FALSE,
            NormalPagePriority | MdlMappingNoExecute
        );
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        MmFreePagesFromMdl(mdl);
        IoFreeMdl(mdl);
        return kstd::expected<SharedSessionDescriptor>::error(GetExceptionCode());
    }

    if (!userVa) {
        MmFreePagesFromMdl(mdl);
        IoFreeMdl(mdl);
        return kstd::expected<SharedSessionDescriptor>::error(STATUS_INSUFFICIENT_RESOURCES);
    }

    auto* node = static_cast<SessionListNode*>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(SessionListNode), 'NDPU')
    );

    if (!node) {
        MmUnmapLockedPages(userVa, mdl);
        MmFreePagesFromMdl(mdl);
        IoFreeMdl(mdl);
        return kstd::expected<SharedSessionDescriptor>::error(STATUS_INSUFFICIENT_RESOURCES);
    }

    PEPROCESS currentProc = PsGetCurrentProcess();
    ObReferenceObject(currentProc);

    SharedSessionDescriptor desc{};
    desc.SessionId = m_nextSessionId++;
    desc.Mode = MemoryMode::PhysicalMdlZeroCopy;
    desc.Mdl = mdl;
    desc.KernelVa = NULL;
    desc.UserVa = userVa;
    desc.TotalBytes = totalBytes;
    desc.PageCount = pageCount;
    desc.ActiveBufferIndex = 0;
    desc.SwapCounter = 0;
    desc.SectionHandle = NULL;
    desc.OwningProcess = currentProc;

    node->Descriptor = desc;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);
    InsertTailList(&m_sessionListHead, &node->ListEntry);
    KeReleaseSpinLock(&m_lock, oldIrql);

    return desc;
}

NTSTATUS MdlMemoryEngine::FreeSharedSession(uint64_t sessionId) noexcept {
    if (sessionId == 0) return STATUS_INVALID_PARAMETER;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    SessionListNode* targetNode = nullptr;
    for (PLIST_ENTRY curr = m_sessionListHead.Flink; curr != &m_sessionListHead; curr = curr->Flink) {
        auto* node = CONTAINING_RECORD(curr, SessionListNode, ListEntry);
        if (node->Descriptor.SessionId == sessionId) {
            RemoveEntryList(&node->ListEntry);
            targetNode = node;
            break;
        }
    }

    KeReleaseSpinLock(&m_lock, oldIrql);

    if (!targetNode) {
        return STATUS_NOT_FOUND;
    }

    SharedSessionDescriptor desc = targetNode->Descriptor;

    if (desc.UserVa && desc.Mdl) {
        PEPROCESS currentProcess = PsGetCurrentProcess();
        if (desc.OwningProcess && desc.OwningProcess != currentProcess) {
            unpd::mmu::ProcessAttachmentGuard guard(desc.OwningProcess);
            MmUnmapLockedPages(desc.UserVa, desc.Mdl);
        } else {
            MmUnmapLockedPages(desc.UserVa, desc.Mdl);
        }
    }

    if (desc.Mdl) {
        MmFreePagesFromMdl(desc.Mdl);
        IoFreeMdl(desc.Mdl);
    }

    if (desc.OwningProcess) {
        ObDereferenceObject(desc.OwningProcess);
    }

    ExFreePoolWithTag(targetNode, 'NDPU');
    return STATUS_SUCCESS;
}

NTSTATUS MdlMemoryEngine::SwapBuffers(
    uint64_t sessionId,
    uint32_t& outActive,
    uint32_t& outStandby,
    uint64_t& outSwaps
) noexcept {
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    for (PLIST_ENTRY curr = m_sessionListHead.Flink; curr != &m_sessionListHead; curr = curr->Flink) {
        auto* node = CONTAINING_RECORD(curr, SessionListNode, ListEntry);
        if (node->Descriptor.SessionId == sessionId) {
            LONG current = node->Descriptor.ActiveBufferIndex;
            LONG next = (current == 0) ? 1 : 0;
            InterlockedExchange(&node->Descriptor.ActiveBufferIndex, next);
            UnpdMemoryFence();

            LONG64 totalSwaps = InterlockedIncrement64(&node->Descriptor.SwapCounter);

            outActive = static_cast<uint32_t>(next);
            outStandby = static_cast<uint32_t>(current);
            outSwaps = static_cast<uint64_t>(totalSwaps);

            KeReleaseSpinLock(&m_lock, oldIrql);
            return STATUS_SUCCESS;
        }
    }

    KeReleaseSpinLock(&m_lock, oldIrql);
    return STATUS_NOT_FOUND;
}

// ============================================================================
// SlabMemoryEngine Implementation
// ============================================================================

SlabMemoryEngine::SlabMemoryEngine() noexcept
    : m_initialized(false) {}

SlabMemoryEngine::~SlabMemoryEngine() noexcept {
    Shutdown();
}

NTSTATUS SlabMemoryEngine::Initialize() noexcept {
    if (m_initialized) return STATUS_SUCCESS;

    for (size_t i = 0; i < SLAB_CLASSES; ++i) {
        ExInitializeNPagedLookasideList(
            &m_slabLookaside[i],
            NULL,
            NULL,
            POOL_FLAG_NON_PAGED,
            SLAB_SIZES[i],
            SLAB_TAGS[i],
            0
        );
    }

    m_initialized = true;
    return STATUS_SUCCESS;
}

void SlabMemoryEngine::Shutdown() noexcept {
    if (!m_initialized) return;

    for (size_t i = 0; i < SLAB_CLASSES; ++i) {
        ExDeleteNPagedLookasideList(&m_slabLookaside[i]);
    }

    m_initialized = false;
}

kstd::expected<uint64_t> SlabMemoryEngine::AllocateSlab(uint32_t blockClass, uint32_t& outBlockSize) noexcept {
    if (blockClass >= SLAB_CLASSES) {
        return kstd::expected<uint64_t>::error(STATUS_INVALID_PARAMETER);
    }

    PVOID ptr = ExAllocateFromNPagedLookasideList(&m_slabLookaside[blockClass]);
    if (!ptr) {
        return kstd::expected<uint64_t>::error(STATUS_INSUFFICIENT_RESOURCES);
    }

    outBlockSize = SLAB_SIZES[blockClass];
    return reinterpret_cast<uint64_t>(ptr);
}

NTSTATUS SlabMemoryEngine::FreeSlab(uint64_t slabHandle, uint32_t blockSize) noexcept {
    if (slabHandle == 0) return STATUS_INVALID_PARAMETER;

    for (size_t i = 0; i < SLAB_CLASSES; ++i) {
        if (SLAB_SIZES[i] == blockSize) {
            PVOID ptr = reinterpret_cast<PVOID>(slabHandle);
            ExFreeToNPagedLookasideList(&m_slabLookaside[i], ptr);
            return STATUS_SUCCESS;
        }
    }
    return STATUS_INVALID_PARAMETER;
}

// ============================================================================
// PoolMemoryEngine Implementation
// ============================================================================

PoolMemoryEngine::PoolMemoryEngine() noexcept
    : m_initialized(false) {}

PoolMemoryEngine::~PoolMemoryEngine() noexcept {
    Shutdown();
}

NTSTATUS PoolMemoryEngine::Initialize() noexcept {
    m_initialized = true;
    return STATUS_SUCCESS;
}

void PoolMemoryEngine::Shutdown() noexcept {
    m_initialized = false;
}

kstd::expected<uint64_t> PoolMemoryEngine::AllocatePoolBlock(SIZE_T size, ULONG tag) noexcept {
    if (size == 0 || size > 1024ULL * 1024 * 1024) {
        return kstd::expected<uint64_t>::error(STATUS_INVALID_PARAMETER);
    }

    PVOID block = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, tag);
    if (!block) {
        return kstd::expected<uint64_t>::error(STATUS_INSUFFICIENT_RESOURCES);
    }

    return reinterpret_cast<uint64_t>(block);
}

NTSTATUS PoolMemoryEngine::FreePoolBlock(uint64_t handle) noexcept {
    if (handle == 0) return STATUS_INVALID_PARAMETER;
    PVOID block = reinterpret_cast<PVOID>(handle);
    ExFreePoolWithTag(block, UNPD_POOL_TAG);
    return STATUS_SUCCESS;
}

// ============================================================================
// DirectNeitherEngine Implementation
// ============================================================================

NTSTATUS DirectNeitherEngine::ValidateUserBuffer(PVOID userPtr, SIZE_T length, bool writeAccess) noexcept {
    if (!userPtr || length == 0) return STATUS_INVALID_PARAMETER;

    __try {
        if (writeAccess) {
            ProbeForWrite(userPtr, length, sizeof(ULONG));
        } else {
            ProbeForRead(userPtr, length, sizeof(ULONG));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
    return STATUS_SUCCESS;
}

// ============================================================================
// UniversalMemoryManager Implementation
// ============================================================================

UniversalMemoryManager::UniversalMemoryManager() noexcept
    : m_initialized(false) {}

UniversalMemoryManager::~UniversalMemoryManager() noexcept {
    Shutdown();
}

NTSTATUS UniversalMemoryManager::Initialize() noexcept {
    if (m_initialized) return STATUS_SUCCESS;

    NTSTATUS status = m_mdlEngine.Initialize();
    if (!NT_SUCCESS(status)) return status;

    status = m_slabEngine.Initialize();
    if (!NT_SUCCESS(status)) return status;

    status = m_poolEngine.Initialize();
    if (!NT_SUCCESS(status)) return status;

    m_initialized = true;
    return STATUS_SUCCESS;
}

void UniversalMemoryManager::Shutdown() noexcept {
    if (!m_initialized) return;

    m_mdlEngine.Shutdown();
    m_slabEngine.Shutdown();
    m_poolEngine.Shutdown();

    m_initialized = false;
}

} // namespace unpd::memory

#endif // _KERNEL_MODE
