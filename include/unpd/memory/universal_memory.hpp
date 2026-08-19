#pragma once

#ifndef UNPD_UNIVERSAL_MEMORY_HPP
#define UNPD_UNIVERSAL_MEMORY_HPP

#ifdef _KERNEL_MODE
#include <ntddk.h>
#include "unpd/common.h"
#include "unpd/kernel_raii.hpp"
#include "unpd/kstd/kstd_span.hpp"
#include "unpd/kstd/kstd_expected.hpp"

namespace unpd::memory {

/**
 * @brief Memory allocation and sharing modes supported by the universal engine.
 */
enum class MemoryMode : uint32_t {
    PhysicalMdlZeroCopy = 0, ///< Physical page allocation mapped to user virtual space
    SystemPoolNonPaged  = 1, ///< NonPagedPoolNx tagged pool allocation
    KernelSectionShared = 2, ///< Section object mapped across user/kernel boundaries
    DirectNeitherBuffer = 3, ///< Direct I/O locked pages or probed Neither I/O user pointers
    SlabCachePool       = 4  ///< Fixed-size Lookaside List cache pools (64B..4KB)
};

/**
 * @brief Configuration descriptor for universal shared memory sessions.
 */
struct SharedSessionDescriptor {
    uint64_t SessionId;
    MemoryMode Mode;
    PMDL Mdl;
    PVOID KernelVa;
    PVOID UserVa;
    SIZE_T TotalBytes;
    ULONG PageCount;
    volatile LONG ActiveBufferIndex;
    volatile LONG64 SwapCounter;
    HANDLE SectionHandle;
};

/**
 * @brief Universal Kernel Memory Management Subsystem.
 *
 * @details
 * Combines zero-copy physical MDL mapping, section objects, tracked non-paged pool,
 * and lockless slab caches into a modular, unified architecture.
 */
class UniversalMemoryManager {
public:
    UniversalMemoryManager() noexcept;
    ~UniversalMemoryManager() noexcept;

    UniversalMemoryManager(const UniversalMemoryManager&) = delete;
    UniversalMemoryManager& operator=(const UniversalMemoryManager&) = delete;

    /**
     * @brief Initializes the universal memory manager and lookaside lists.
     * @irql_requirement PASSIVE_LEVEL
     */
    NTSTATUS Initialize() noexcept;

    /**
     * @brief Shuts down the memory manager and cleans up all active sessions and slabs.
     * @irql_requirement PASSIVE_LEVEL
     */
    void Shutdown() noexcept;

    /**
     * @brief Allocates and maps zero-copy physical MDL pages into user space.
     * @irql_requirement PASSIVE_LEVEL
     */
    kstd::expected<SharedSessionDescriptor> AllocateMdlSharedSession(ULONG pageCount) noexcept;

    /**
     * @brief Unmaps and releases an active MDL shared memory session.
     * @irql_requirement PASSIVE_LEVEL
     */
    NTSTATUS FreeMdlSharedSession(uint64_t sessionId) noexcept;

    /**
     * @brief Performs an atomic lock-free buffer swap for a shared memory session.
     * @irql_requirement <= DISPATCH_LEVEL
     */
    NTSTATUS SwapBuffers(uint64_t sessionId, uint32_t& outActive, uint32_t& outStandby, uint64_t& outSwaps) noexcept;

    /**
     * @brief Allocates from the tagged NonPagedPoolNx pool with handle tracking.
     * @irql_requirement <= DISPATCH_LEVEL
     */
    kstd::expected<uint64_t> AllocatePoolBlock(SIZE_T size, ULONG tag = UNPD_POOL_TAG) noexcept;

    /**
     * @brief Frees a tracked NonPagedPoolNx pool allocation by handle.
     * @irql_requirement <= DISPATCH_LEVEL
     */
    NTSTATUS FreePoolBlock(uint64_t handle) noexcept;

    /**
     * @brief Allocates a block from the fixed-size slab cache (64B, 256B, 1KB, 4KB).
     * @irql_requirement <= DISPATCH_LEVEL
     */
    kstd::expected<uint64_t> AllocateSlab(uint32_t blockClass, uint32_t& outBlockSize) noexcept;

    /**
     * @brief Returns a slab block to the appropriate lookaside free-list.
     * @irql_requirement <= DISPATCH_LEVEL
     */
    NTSTATUS FreeSlab(uint64_t slabHandle, uint32_t blockSize) noexcept;

    /**
     * @brief Probes and verifies user buffer alignment and readability inside SEH.
     * @irql_requirement PASSIVE_LEVEL
     */
    static NTSTATUS ValidateUserBuffer(PVOID userPtr, SIZE_T length, bool writeAccess) noexcept;

private:
    static constexpr size_t MAX_SESSIONS = 32;
    static constexpr size_t SLAB_CLASSES = 4;

    KSPIN_LOCK m_lock;
    SharedSessionDescriptor m_sessions[MAX_SESSIONS];
    NPAGED_LOOKASIDE_LIST m_slabLookaside[SLAB_CLASSES];
    bool m_initialized;
    uint64_t m_nextSessionId;
};

} // namespace unpd::memory

#endif // _KERNEL_MODE
#endif // UNPD_UNIVERSAL_MEMORY_HPP
