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
// MdlMemoryEngine Implementation (Dynamic Sessions, Refcounting & Context Invariance)
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

#ifdef _KERNEL_MODE
static void SafeUnmapUserVa(PVOID userVa, PMDL mdl) noexcept {
    if (!userVa || !mdl) return;
    __try {
        MmUnmapLockedPages(userVa, mdl);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
#endif

void MdlMemoryEngine::DestroySessionNode(SessionListNode* node) noexcept {
    if (!node) return;

    SharedSessionDescriptor& desc = node->Descriptor;

    if (desc.UserVa && desc.Mdl) {
        PEPROCESS currentProcess = PsGetCurrentProcess();
        if (desc.OwningProcess && desc.OwningProcess != currentProcess) {
            unpd::mmu::ProcessAttachmentGuard guard(desc.OwningProcess);
#ifdef _KERNEL_MODE
            if (desc.SecureHandle) {
                MmUnsecureVirtualMemory(desc.SecureHandle);
                desc.SecureHandle = NULL;
            }
#endif
            SafeUnmapUserVa(desc.UserVa, desc.Mdl);
        } else {
#ifdef _KERNEL_MODE
            if (desc.SecureHandle) {
                MmUnsecureVirtualMemory(desc.SecureHandle);
                desc.SecureHandle = NULL;
            }
#endif
            SafeUnmapUserVa(desc.UserVa, desc.Mdl);
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
}

void MdlMemoryEngine::Shutdown() noexcept {
    if (!m_initialized) return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    while (!IsListEmpty(&m_sessionListHead)) {
        PLIST_ENTRY entry = RemoveHeadList(&m_sessionListHead);
        auto* node = CONTAINING_RECORD(entry, SessionListNode, ListEntry);
        InterlockedExchange(&node->Descriptor.IsTornDown, 1);
        KeReleaseSpinLock(&m_lock, oldIrql);

        LONG refs = InterlockedDecrement(&node->Descriptor.ReferenceCount);
        if (refs == 0) {
            DestroySessionNode(node);
        }

        KeAcquireSpinLock(&m_lock, &oldIrql);
    }

    KeReleaseSpinLock(&m_lock, oldIrql);
    m_initialized = false;
}

SessionListNode* MdlMemoryEngine::AcquireSessionReference(uint64_t sessionId) noexcept {
    if (sessionId == 0) return nullptr;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    for (PLIST_ENTRY curr = m_sessionListHead.Flink; curr != &m_sessionListHead; curr = curr->Flink) {
        auto* node = CONTAINING_RECORD(curr, SessionListNode, ListEntry);
        if (node->Descriptor.SessionId == sessionId && node->Descriptor.IsTornDown == 0) {
            InterlockedIncrement(&node->Descriptor.ReferenceCount);
            KeReleaseSpinLock(&m_lock, oldIrql);
            return node;
        }
    }

    KeReleaseSpinLock(&m_lock, oldIrql);
    return nullptr;
}

void MdlMemoryEngine::ReleaseSessionReference(SessionListNode* node) noexcept {
    if (!node) return;

    LONG refs = InterlockedDecrement(&node->Descriptor.ReferenceCount);
    if (refs == 0 && node->Descriptor.IsTornDown == 1) {
        DestroySessionNode(node);
    }
}

void MdlMemoryEngine::HandleProcessExit(HANDLE processId) noexcept {
    if (!processId) return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    for (PLIST_ENTRY curr = m_sessionListHead.Flink; curr != &m_sessionListHead; curr = curr->Flink) {
        auto* node = CONTAINING_RECORD(curr, SessionListNode, ListEntry);
        if (node->Descriptor.OwningProcess) {
            HANDLE pid = PsGetProcessId(node->Descriptor.OwningProcess);
            if (pid == processId) {
                // Windows automatically cleans up the dying process's VAD tree upon exit.
                // Nullify UserVa and SecureHandle to safely prevent double-unmap attempts in DestroySessionNode / DriverUnload.
                if (node->Descriptor.UserVa && node->Descriptor.Mdl) {
                    node->Descriptor.UserVa = NULL;
                    node->Descriptor.SecureHandle = NULL;
                }
            }
        }
    }

    KeReleaseSpinLock(&m_lock, oldIrql);
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

    PVOID secureHandle = NULL;
#ifdef _KERNEL_MODE
    secureHandle = MmSecureVirtualMemory(userVa, totalBytes, PAGE_READWRITE);
    if (!secureHandle) {
        MmUnmapLockedPages(userVa, mdl);
        MmFreePagesFromMdl(mdl);
        IoFreeMdl(mdl);
        return kstd::expected<SharedSessionDescriptor>::error(STATUS_INSUFFICIENT_RESOURCES);
    }
#endif

    auto* node = static_cast<SessionListNode*>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(SessionListNode), 'NDPU')
    );

    if (!node) {
#ifdef _KERNEL_MODE
        if (secureHandle) {
            MmUnsecureVirtualMemory(secureHandle);
        }
#endif
        MmUnmapLockedPages(userVa, mdl);
        MmFreePagesFromMdl(mdl);
        IoFreeMdl(mdl);
        return kstd::expected<SharedSessionDescriptor>::error(STATUS_INSUFFICIENT_RESOURCES);
    }

    PEPROCESS currentProc = PsGetCurrentProcess();
    ObReferenceObject(currentProc);

    uint64_t sessionId = static_cast<uint64_t>(InterlockedIncrement64(&m_nextSessionId));

    SharedSessionDescriptor desc{};
    desc.SessionId = sessionId;
    desc.Mode = MemoryMode::PhysicalMdlZeroCopy;
    desc.Mdl = mdl;
    desc.KernelVa = NULL;
    desc.UserVa = userVa;
    desc.SecureHandle = secureHandle;
    desc.TotalBytes = totalBytes;
    desc.PageCount = pageCount;
    desc.ActiveBufferIndex = 0;
    desc.SwapCounter = 0;
    desc.SectionHandle = NULL;
    desc.OwningProcess = currentProc;
    desc.ReferenceCount = 1; // Initial reference held by session list
    desc.IsTornDown = 0;

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

    InterlockedExchange(&targetNode->Descriptor.IsTornDown, 1);
    LONG refs = InterlockedDecrement(&targetNode->Descriptor.ReferenceCount);
    if (refs == 0) {
        DestroySessionNode(targetNode);
    }

    return STATUS_SUCCESS;
}

NTSTATUS MdlMemoryEngine::SwapBuffers(
    uint64_t sessionId,
    uint32_t& outActive,
    uint32_t& outStandby,
    uint64_t& outSwaps
) noexcept {
    SessionListNode* node = AcquireSessionReference(sessionId);
    if (!node) {
        return STATUS_NOT_FOUND;
    }

    LONG current = node->Descriptor.ActiveBufferIndex;
    LONG next = (current == 0) ? 1 : 0;
    InterlockedExchange(&node->Descriptor.ActiveBufferIndex, next);
    UnpdMemoryFence();

    LONG64 totalSwaps = InterlockedIncrement64(&node->Descriptor.SwapCounter);

    outActive = static_cast<uint32_t>(next);
    outStandby = static_cast<uint32_t>(current);
    outSwaps = static_cast<uint64_t>(totalSwaps);

    ReleaseSessionReference(node);
    return STATUS_SUCCESS;
}

// ============================================================================
// SlabMemoryEngine Implementation (Opaque Tokens & Double-Free Protection)
// ============================================================================

SlabMemoryEngine::SlabMemoryEngine() noexcept
    : m_nextSlot(0), m_initialized(false) {
    KeInitializeSpinLock(&m_tableLock);
    for (size_t i = 0; i < MAX_SLAB_HANDLES; ++i) {
        m_handleTable[i].BlockAddress = nullptr;
        m_handleTable[i].BlockClass = 0;
        m_handleTable[i].Generation = 1;
        m_handleTable[i].InUse = 0;
    }
}

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

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_tableLock, &oldIrql);

    for (size_t slot = 0; slot < MAX_SLAB_HANDLES; ++slot) {
        if (m_handleTable[slot].InUse == 1 && m_handleTable[slot].BlockAddress) {
            uint32_t bClass = m_handleTable[slot].BlockClass;
            PVOID blockPtr = m_handleTable[slot].BlockAddress;
            m_handleTable[slot].BlockAddress = nullptr;
            m_handleTable[slot].InUse = 0;

            if (bClass < SLAB_CLASSES) {
                ExFreeToNPagedLookasideList(&m_slabLookaside[bClass], blockPtr);
            }
        }
    }

    KeReleaseSpinLock(&m_tableLock, oldIrql);

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

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_tableLock, &oldIrql);

    uint32_t foundSlot = MAX_SLAB_HANDLES;
    for (size_t i = 0; i < MAX_SLAB_HANDLES; ++i) {
        uint32_t slot = (static_cast<uint32_t>(m_nextSlot) + static_cast<uint32_t>(i)) % MAX_SLAB_HANDLES;
        if (m_handleTable[slot].InUse == 0) {
            foundSlot = slot;
            m_nextSlot = (slot + 1) % MAX_SLAB_HANDLES;
            break;
        }
    }

    if (foundSlot >= MAX_SLAB_HANDLES) {
        KeReleaseSpinLock(&m_tableLock, oldIrql);
        ExFreeToNPagedLookasideList(&m_slabLookaside[blockClass], ptr);
        return kstd::expected<uint64_t>::error(STATUS_INSUFFICIENT_RESOURCES);
    }

    m_handleTable[foundSlot].BlockAddress = ptr;
    m_handleTable[foundSlot].BlockClass = blockClass;
    m_handleTable[foundSlot].InUse = 1;
    uint32_t gen = m_handleTable[foundSlot].Generation;

    KeReleaseSpinLock(&m_tableLock, oldIrql);

    outBlockSize = SLAB_SIZES[blockClass];

    // Encode Opaque Token Handle: Class (8b) | Slot (24b) | Generation (32b)
    uint64_t opaqueHandle = (static_cast<uint64_t>(blockClass) << 56) |
                            (static_cast<uint64_t>(foundSlot & 0xFFFFFF) << 32) |
                            (static_cast<uint64_t>(gen));

    return opaqueHandle;
}

NTSTATUS SlabMemoryEngine::FreeSlab(uint64_t slabHandle, uint32_t blockSize) noexcept {
    if (slabHandle == 0) return STATUS_INVALID_PARAMETER;

    uint32_t blockClass = static_cast<uint32_t>((slabHandle >> 56) & 0xFF);
    uint32_t slot = static_cast<uint32_t>((slabHandle >> 32) & 0xFFFFFF);
    uint32_t gen = static_cast<uint32_t>(slabHandle & 0xFFFFFFFF);

    if (blockClass >= SLAB_CLASSES || slot >= MAX_SLAB_HANDLES) {
        return STATUS_INVALID_HANDLE;
    }

    if (SLAB_SIZES[blockClass] != blockSize) {
        return STATUS_INVALID_PARAMETER;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_tableLock, &oldIrql);

    if (m_handleTable[slot].InUse == 0 ||
        m_handleTable[slot].Generation != gen ||
        m_handleTable[slot].BlockClass != blockClass ||
        m_handleTable[slot].BlockAddress == nullptr) {
        KeReleaseSpinLock(&m_tableLock, oldIrql);
        return STATUS_INVALID_HANDLE; // Detects Double Free and Stale Handles safely
    }

    PVOID blockPtr = m_handleTable[slot].BlockAddress;
    m_handleTable[slot].BlockAddress = nullptr;
    m_handleTable[slot].InUse = 0;
    m_handleTable[slot].Generation = (m_handleTable[slot].Generation == 0xFFFFFFFF) ? 1 : (m_handleTable[slot].Generation + 1);

    KeReleaseSpinLock(&m_tableLock, oldIrql);

    ExFreeToNPagedLookasideList(&m_slabLookaside[blockClass], blockPtr);
    return STATUS_SUCCESS;
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
