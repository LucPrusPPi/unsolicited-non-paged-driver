#include "unpd/memory/universal_memory.hpp"
#include "unpd/kernel_asm.hpp"

#ifdef _KERNEL_MODE

namespace unpd::memory {

static constexpr ULONG SLAB_SIZES[4] = { 64, 256, 1024, 4096 };
static constexpr ULONG SLAB_TAGS[4]  = { '1LSU', '2LSU', '3LSU', '4LSU' };

UniversalMemoryManager::UniversalMemoryManager() noexcept
    : m_initialized(false), m_nextSessionId(1) {
    KeInitializeSpinLock(&m_lock);
    RtlZeroMemory(m_sessions, sizeof(m_sessions));
}

UniversalMemoryManager::~UniversalMemoryManager() noexcept {
    Shutdown();
}

/**
 * @brief Initializes lookaside lists for fixed-size fast slab allocation.
 *
 * @details
 * - API: ExInitializeNPagedLookasideList
 * - IRQL Requirement: <= DISPATCH_LEVEL
 * - Memory Safety: Allocates NonPagedPoolNx blocks with unique pool tags per class
 */
NTSTATUS UniversalMemoryManager::Initialize() noexcept {
    if (m_initialized) return STATUS_SUCCESS;

    for (size_t i = 0; i < SLAB_CLASSES; ++i) {
        ExInitializeNPagedLookasideList(
            &m_slabLookaside[i],
            NULL, // Default allocate function
            NULL, // Default free function
            POOL_FLAG_NON_PAGED,
            SLAB_SIZES[i],
            SLAB_TAGS[i],
            0 // System-managed depth
        );
    }

    m_initialized = true;
    return STATUS_SUCCESS;
}

/**
 * @brief Releases all active shared memory sessions and lookaside lists.
 *
 * @details
 * - API: ExDeleteNPagedLookasideList, MmUnmapLockedPages, MmFreePagesFromMdl, IoFreeMdl
 * - IRQL Requirement: PASSIVE_LEVEL (required for MmUnmapLockedPages)
 */
void UniversalMemoryManager::Shutdown() noexcept {
    if (!m_initialized) return;

    for (size_t i = 0; i < MAX_SESSIONS; ++i) {
        if (m_sessions[i].SessionId != 0) {
            if (m_sessions[i].UserVa && m_sessions[i].Mdl) {
                MmUnmapLockedPages(m_sessions[i].UserVa, m_sessions[i].Mdl);
                m_sessions[i].UserVa = NULL;
            }
            if (m_sessions[i].Mdl) {
                MmFreePagesFromMdl(m_sessions[i].Mdl);
                IoFreeMdl(m_sessions[i].Mdl);
                m_sessions[i].Mdl = NULL;
            }
            m_sessions[i].SessionId = 0;
        }
    }

    for (size_t i = 0; i < SLAB_CLASSES; ++i) {
        ExDeleteNPagedLookasideList(&m_slabLookaside[i]);
    }

    m_initialized = false;
}

/**
 * @brief Allocates physical RAM pages and maps them to user mode without kernel copying.
 *
 * @details
 * - APIs: MmAllocatePagesForMdlEx, MmMapLockedPagesSpecifyCache
 * - IRQL: PASSIVE_LEVEL
 * - W^X Security: Uses MdlMappingNoExecute
 */
kstd::expected<SharedSessionDescriptor> UniversalMemoryManager::AllocateMdlSharedSession(ULONG pageCount) noexcept {
    if (pageCount == 0 || pageCount > 256) {
        return kstd::expected<SharedSessionDescriptor>::error(STATUS_INVALID_PARAMETER);
    }

    SIZE_T totalBytes = static_cast<SIZE_T>(pageCount) * PAGE_SIZE;
    PHYSICAL_ADDRESS lowAddr{};
    PHYSICAL_ADDRESS highAddr{};
    PHYSICAL_ADDRESS skipBytes{};
    highAddr.QuadPart = MAXULONG64;

    // Allocate physical memory pages
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

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    size_t freeIdx = MAX_SESSIONS;
    for (size_t i = 0; i < MAX_SESSIONS; ++i) {
        if (m_sessions[i].SessionId == 0) {
            freeIdx = i;
            break;
        }
    }

    if (freeIdx == MAX_SESSIONS) {
        KeReleaseSpinLock(&m_lock, oldIrql);
        MmUnmapLockedPages(userVa, mdl);
        MmFreePagesFromMdl(mdl);
        IoFreeMdl(mdl);
        return kstd::expected<SharedSessionDescriptor>::error(STATUS_TOO_MANY_SESSIONS);
    }

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

    m_sessions[freeIdx] = desc;
    KeReleaseSpinLock(&m_lock, oldIrql);

    return desc;
}

NTSTATUS UniversalMemoryManager::FreeMdlSharedSession(uint64_t sessionId) noexcept {
    if (sessionId == 0) return STATUS_INVALID_PARAMETER;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    size_t foundIdx = MAX_SESSIONS;
    for (size_t i = 0; i < MAX_SESSIONS; ++i) {
        if (m_sessions[i].SessionId == sessionId) {
            foundIdx = i;
            break;
        }
    }

    if (foundIdx == MAX_SESSIONS) {
        KeReleaseSpinLock(&m_lock, oldIrql);
        return STATUS_NOT_FOUND;
    }

    SharedSessionDescriptor sessionToFree = m_sessions[foundIdx];
    m_sessions[foundIdx].SessionId = 0;
    KeReleaseSpinLock(&m_lock, oldIrql);

    if (sessionToFree.UserVa && sessionToFree.Mdl) {
        MmUnmapLockedPages(sessionToFree.UserVa, sessionToFree.Mdl);
    }
    if (sessionToFree.Mdl) {
        MmFreePagesFromMdl(sessionToFree.Mdl);
        IoFreeMdl(sessionToFree.Mdl);
    }

    return STATUS_SUCCESS;
}

NTSTATUS UniversalMemoryManager::SwapBuffers(
    uint64_t sessionId,
    uint32_t& outActive,
    uint32_t& outStandby,
    uint64_t& outSwaps
) noexcept {
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_lock, &oldIrql);

    for (size_t i = 0; i < MAX_SESSIONS; ++i) {
        if (m_sessions[i].SessionId == sessionId) {
            LONG current = m_sessions[i].ActiveBufferIndex;
            LONG next = (current == 0) ? 1 : 0;
            InterlockedExchange(&m_sessions[i].ActiveBufferIndex, next);
            UnpdMemoryFence();

            LONG64 totalSwaps = InterlockedIncrement64(&m_sessions[i].SwapCounter);

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

kstd::expected<uint64_t> UniversalMemoryManager::AllocatePoolBlock(SIZE_T size, ULONG tag) noexcept {
    if (size == 0 || size > 1024ULL * 1024 * 1024) {
        return kstd::expected<uint64_t>::error(STATUS_INVALID_PARAMETER);
    }

    PVOID block = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, tag);
    if (!block) {
        return kstd::expected<uint64_t>::error(STATUS_INSUFFICIENT_RESOURCES);
    }

    return reinterpret_cast<uint64_t>(block);
}

NTSTATUS UniversalMemoryManager::FreePoolBlock(uint64_t handle) noexcept {
    if (handle == 0) return STATUS_INVALID_PARAMETER;
    PVOID block = reinterpret_cast<PVOID>(handle);
    ExFreePoolWithTag(block, UNPD_POOL_TAG);
    return STATUS_SUCCESS;
}

kstd::expected<uint64_t> UniversalMemoryManager::AllocateSlab(uint32_t blockClass, uint32_t& outBlockSize) noexcept {
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

NTSTATUS UniversalMemoryManager::FreeSlab(uint64_t slabHandle, uint32_t blockSize) noexcept {
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

NTSTATUS UniversalMemoryManager::ValidateUserBuffer(PVOID userPtr, SIZE_T length, bool writeAccess) noexcept {
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

} // namespace unpd::memory

#endif // _KERNEL_MODE
