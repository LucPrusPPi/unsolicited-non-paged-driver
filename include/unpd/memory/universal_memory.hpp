#pragma once

#ifndef UNPD_UNIVERSAL_MEMORY_HPP
#define UNPD_UNIVERSAL_MEMORY_HPP

#ifdef _KERNEL_MODE
#include <ntddk.h>
#include "unpd/common.h"
#include "unpd/kernel_raii.hpp"
#include "unpd/kstd/span.hpp"
#include "unpd/kstd/expected.hpp"

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
    PEPROCESS OwningProcess;
    volatile LONG ReferenceCount; ///< Atomic reference counter to prevent UAF
    volatile LONG IsTornDown;     ///< Flag set when session teardown has started
};

/**
 * @brief Intrusive dynamic list node for tracking active sessions.
 */
struct SessionListNode {
    LIST_ENTRY ListEntry;
    SharedSessionDescriptor Descriptor;
};

/**
 * @brief Abstract Polymorphic Interface for Kernel Memory Engines.
 */
class IMemoryEngine {
public:
    virtual ~IMemoryEngine() = default;

    /**
     * @brief Initializes underlying pool, lists, or hardware descriptors.
     * @irql_requirement <= DISPATCH_LEVEL
     */
    virtual NTSTATUS Initialize() noexcept = 0;

    /**
     * @brief Cleans up and releases all resources managed by the engine.
     * @irql_requirement PASSIVE_LEVEL
     */
    virtual void Shutdown() noexcept = 0;

    /**
     * @brief Returns the memory operational mode identifier.
     */
    [[nodiscard]] virtual MemoryMode GetMode() const noexcept = 0;

    /**
     * @brief Returns human-readable engine name.
     */
    [[nodiscard]] virtual const char* GetName() const noexcept = 0;
};

/**
 * @brief Physical Memory MDL Zero-Copy Engine with dynamic session tracking and process isolation.
 */
class MdlMemoryEngine final : public IMemoryEngine {
public:
    MdlMemoryEngine() noexcept;
    ~MdlMemoryEngine() noexcept override;

    NTSTATUS Initialize() noexcept override;
    void Shutdown() noexcept override;
    [[nodiscard]] MemoryMode GetMode() const noexcept override { return MemoryMode::PhysicalMdlZeroCopy; }
    [[nodiscard]] const char* GetName() const noexcept override { return "MdlZeroCopyEngine"; }

    kstd::expected<SharedSessionDescriptor> AllocateSharedSession(ULONG pageCount) noexcept;
    NTSTATUS FreeSharedSession(uint64_t sessionId) noexcept;
    NTSTATUS SwapBuffers(uint64_t sessionId, uint32_t& outActive, uint32_t& outStandby, uint64_t& outSwaps) noexcept;

    SessionListNode* AcquireSessionReference(uint64_t sessionId) noexcept;
    void ReleaseSessionReference(SessionListNode* node) noexcept;

private:
    void DestroySessionNode(SessionListNode* node) noexcept;

    KSPIN_LOCK m_lock;
    LIST_ENTRY m_sessionListHead;
    bool m_initialized;
    volatile LONG64 m_nextSessionId;
};

/**
 * @brief High-Speed Lookaside Slab Cache Engine (64B, 256B, 1024B, 4096B).
 */
class SlabMemoryEngine final : public IMemoryEngine {
public:
    SlabMemoryEngine() noexcept;
    ~SlabMemoryEngine() noexcept override;

    NTSTATUS Initialize() noexcept override;
    void Shutdown() noexcept override;
    [[nodiscard]] MemoryMode GetMode() const noexcept override { return MemoryMode::SlabCachePool; }
    [[nodiscard]] const char* GetName() const noexcept override { return "SlabCacheEngine"; }

    kstd::expected<uint64_t> AllocateSlab(uint32_t blockClass, uint32_t& outBlockSize) noexcept;
    NTSTATUS FreeSlab(uint64_t slabHandle, uint32_t blockSize) noexcept;

private:
    static constexpr size_t SLAB_CLASSES = 4;
    static constexpr size_t MAX_SLAB_HANDLES = 1024;

    struct SlabHandleEntry {
        PVOID BlockAddress;
        uint32_t BlockClass;
        uint32_t Generation;
        volatile LONG InUse;
    };

    NPAGED_LOOKASIDE_LIST m_slabLookaside[SLAB_CLASSES];
    SlabHandleEntry m_handleTable[MAX_SLAB_HANDLES];
    KSPIN_LOCK m_tableLock;
    volatile LONG m_nextSlot;
    bool m_initialized;
};

/**
 * @brief Tracked Non-Paged Pool Memory Engine.
 */
class PoolMemoryEngine final : public IMemoryEngine {
public:
    PoolMemoryEngine() noexcept;
    ~PoolMemoryEngine() noexcept override;

    NTSTATUS Initialize() noexcept override;
    void Shutdown() noexcept override;
    [[nodiscard]] MemoryMode GetMode() const noexcept override { return MemoryMode::SystemPoolNonPaged; }
    [[nodiscard]] const char* GetName() const noexcept override { return "NonPagedPoolEngine"; }

    kstd::expected<uint64_t> AllocatePoolBlock(SIZE_T size, ULONG tag = UNPD_POOL_TAG) noexcept;
    NTSTATUS FreePoolBlock(uint64_t handle) noexcept;

private:
    bool m_initialized;
};

/**
 * @brief Direct & Probed User Buffer Memory Engine.
 */
class DirectNeitherEngine final : public IMemoryEngine {
public:
    DirectNeitherEngine() noexcept = default;
    ~DirectNeitherEngine() noexcept override = default;

    NTSTATUS Initialize() noexcept override { return STATUS_SUCCESS; }
    void Shutdown() noexcept override {}
    [[nodiscard]] MemoryMode GetMode() const noexcept override { return MemoryMode::DirectNeitherBuffer; }
    [[nodiscard]] const char* GetName() const noexcept override { return "DirectNeitherEngine"; }

    static NTSTATUS ValidateUserBuffer(PVOID userPtr, SIZE_T length, bool writeAccess) noexcept;
};

/**
 * @brief Universal Kernel Memory Management Facade.
 */
class UniversalMemoryManager {
public:
    UniversalMemoryManager() noexcept;
    ~UniversalMemoryManager() noexcept;

    UniversalMemoryManager(const UniversalMemoryManager&) = delete;
    UniversalMemoryManager& operator=(const UniversalMemoryManager&) = delete;

    NTSTATUS Initialize() noexcept;
    void Shutdown() noexcept;

    // MDL Zero-Copy Operations
    kstd::expected<SharedSessionDescriptor> AllocateMdlSharedSession(ULONG pageCount) noexcept {
        return m_mdlEngine.AllocateSharedSession(pageCount);
    }
    NTSTATUS FreeMdlSharedSession(uint64_t sessionId) noexcept {
        return m_mdlEngine.FreeSharedSession(sessionId);
    }
    NTSTATUS SwapBuffers(uint64_t sessionId, uint32_t& outActive, uint32_t& outStandby, uint64_t& outSwaps) noexcept {
        return m_mdlEngine.SwapBuffers(sessionId, outActive, outStandby, outSwaps);
    }

    // Non-Paged Pool Operations
    kstd::expected<uint64_t> AllocatePoolBlock(SIZE_T size, ULONG tag = UNPD_POOL_TAG) noexcept {
        return m_poolEngine.AllocatePoolBlock(size, tag);
    }
    NTSTATUS FreePoolBlock(uint64_t handle) noexcept {
        return m_poolEngine.FreePoolBlock(handle);
    }

    // Slab Cache Operations
    kstd::expected<uint64_t> AllocateSlab(uint32_t blockClass, uint32_t& outBlockSize) noexcept {
        return m_slabEngine.AllocateSlab(blockClass, outBlockSize);
    }
    NTSTATUS FreeSlab(uint64_t slabHandle, uint32_t blockSize) noexcept {
        return m_slabEngine.FreeSlab(slabHandle, blockSize);
    }

    // User Buffer Validation
    static NTSTATUS ValidateUserBuffer(PVOID userPtr, SIZE_T length, bool writeAccess) noexcept {
        return DirectNeitherEngine::ValidateUserBuffer(userPtr, length, writeAccess);
    }

    // Polymorphic Engine Accessors
    [[nodiscard]] MdlMemoryEngine& GetMdlEngine() noexcept { return m_mdlEngine; }
    [[nodiscard]] SlabMemoryEngine& GetSlabEngine() noexcept { return m_slabEngine; }
    [[nodiscard]] PoolMemoryEngine& GetPoolEngine() noexcept { return m_poolEngine; }

private:
    MdlMemoryEngine m_mdlEngine;
    SlabMemoryEngine m_slabEngine;
    PoolMemoryEngine m_poolEngine;
    DirectNeitherEngine m_directEngine;
    bool m_initialized;
};

} // namespace unpd::memory

#endif // _KERNEL_MODE
#endif // UNPD_UNIVERSAL_MEMORY_HPP
