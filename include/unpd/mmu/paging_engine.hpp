#pragma once

#ifndef UNPD_PAGING_ENGINE_HPP
#define UNPD_PAGING_ENGINE_HPP

#include "unpd/mmu/paging_types.hpp"
#include "unpd/kstd/expected.hpp"
#include "unpd/kstd/span.hpp"

#ifdef _KERNEL_MODE
#include <ntddk.h>
#include "unpd/kernel_asm.hpp"

#ifndef _KAPC_STATE_DEFINED
#define _KAPC_STATE_DEFINED
typedef struct _KAPC_STATE {
    LIST_ENTRY ApcListHead[2];
    struct _KPROCESS *Process;
    UCHAR InProgressFlags;
    BOOLEAN KernelApcPending;
    BOOLEAN UserApcPendingAll;
} KAPC_STATE, *PKAPC_STATE, *PRKAPC_STATE;
#endif

extern "C" {
NTSTATUS NTAPI ZwAllocateVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _Inout_ PVOID *BaseAddress,
    _In_ ULONG_PTR ZeroBits,
    _Inout_ PSIZE_T RegionSize,
    _In_ ULONG AllocationType,
    _In_ ULONG Protect
);

NTSTATUS NTAPI ZwFreeVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _Inout_ PVOID *BaseAddress,
    _Inout_ PSIZE_T RegionSize,
    _In_ ULONG FreeType
);

void NTAPI KeStackAttachProcess(
    _Inout_ PRKPROCESS Process,
    _Out_ PRKAPC_STATE ApcState
);

void NTAPI KeUnstackDetachProcess(
    _In_ PRKAPC_STATE ApcState
);
}

namespace unpd::mmu {

/**
 * @brief RAII Guard for attaching to a target process address space.
 *
 * @details
 * - APIs: KeStackAttachProcess, KeUnstackDetachProcess
 * - IRQL Requirement: PASSIVE_LEVEL <= IRQL <= APC_LEVEL
 * - Safety: Automatically restores original thread APC state on scope exit.
 */
class ProcessAttachmentGuard {
public:
    explicit ProcessAttachmentGuard(PEPROCESS process) noexcept
        : m_attached(false) {
        RtlZeroMemory(&m_apcState, sizeof(m_apcState));
        if (process) {
            KeStackAttachProcess(reinterpret_cast<PRKPROCESS>(process), &m_apcState);
            m_attached = true;
        }
    }

    ~ProcessAttachmentGuard() noexcept {
        detach();
    }

    ProcessAttachmentGuard(const ProcessAttachmentGuard&) = delete;
    ProcessAttachmentGuard& operator=(const ProcessAttachmentGuard&) = delete;

    void detach() noexcept {
        if (m_attached) {
            KeUnstackDetachProcess(&m_apcState);
            m_attached = false;
        }
    }

    [[nodiscard]] bool isAttached() const noexcept {
        return m_attached;
    }

private:
    KAPC_STATE m_apcState;
    bool m_attached;
};

/**
 * @brief Translation result containing physical address and page size category.
 */
struct TranslationResult {
    uint64_t PhysicalAddress;
    uint64_t PageSize;
    bool IsPresent;
    bool IsWritable;
    bool IsUserAccessible;
    bool IsExecutable;
};

/**
 * @brief High-level hardware page table walker and process memory engine.
 */
class PagingEngine {
public:
    /**
     * @brief Resolves a virtual address to its underlying physical address.
     * @irql_requirement <= DISPATCH_LEVEL
     */
    static kstd::expected<TranslationResult> TranslateVirtualAddress(
        const void* virtualAddress,
        uint64_t cr3Value = 0
    ) noexcept;

    /**
     * @brief Allocates and commits virtual memory pages inside a target process address space.
     * @irql_requirement PASSIVE_LEVEL
     */
    static kstd::expected<PVOID> AllocateProcessMemory(
        HANDLE processHandle,
        SIZE_T byteCount,
        ULONG protection = PAGE_READWRITE
    ) noexcept;

    /**
     * @brief Frees virtual memory pages inside a target process.
     * @irql_requirement PASSIVE_LEVEL
     */
    static NTSTATUS FreeProcessMemory(
        HANDLE processHandle,
        PVOID baseAddress,
        SIZE_T byteCount
    ) noexcept;

    /**
     * @brief Safe memory copy between process boundaries protected by SEH.
     * @irql_requirement PASSIVE_LEVEL
     */
    static NTSTATUS SafeCopyProcessMemory(
        PEPROCESS sourceProcess,
        const void* sourceAddress,
        PEPROCESS targetProcess,
        void* targetAddress,
        SIZE_T size
    ) noexcept;

    /**
     * @brief Invalidates TLB entry for a single 4KB page.
     * @irql_requirement <= HIGH_LEVEL
     */
    static void InvalidatePage(const void* virtualAddress) noexcept {
        UnpdInvlpg(virtualAddress);
    }

    /**
     * @brief Flushes all non-global TLB entries on the current processor core.
     * @irql_requirement <= HIGH_LEVEL
     */
    static void FlushCoreTlb() noexcept {
        UnpdFlushTlb();
    }
};

} // namespace unpd::mmu

#endif // _KERNEL_MODE
#endif // UNPD_PAGING_ENGINE_HPP
