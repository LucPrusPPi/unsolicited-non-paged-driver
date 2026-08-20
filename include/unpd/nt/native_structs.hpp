#pragma once

#ifndef UNPD_NT_NATIVE_STRUCTS_HPP
#define UNPD_NT_NATIVE_STRUCTS_HPP

#include "unpd/config.hpp"
#include "unpd/common.h"

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
typedef ULONG_PTR EX_PUSH_LOCK;
#ifndef STATUS_NOT_FOUND
#define STATUS_NOT_FOUND ((NTSTATUS)0xC0000225L)
#endif
#endif

namespace unpd::nt {

#pragma pack(push, 8)

// ============================================================================
// RTL_AVL_TREE & VAD (Virtual Address Descriptor) Tree Structures
// ============================================================================

/**
 * @brief Self-balancing AVL Tree Node used by Windows NT Memory Manager for process VAD trees.
 */
typedef struct _UNPD_RTL_BALANCED_NODE {
    struct _UNPD_RTL_BALANCED_NODE* Children[2]; /**< Left [0] and Right [1] child pointers */
    struct _UNPD_RTL_BALANCED_NODE* ParentValue; /**< Pointer to parent node combined with balance flags */
    ULONG Red : 1;                               /**< Red-black tree balance flag (if RB-tree variant used) */
    ULONG Balance : 2;                           /**< AVL balance factor */
} UNPD_RTL_BALANCED_NODE, *PUNPD_RTL_BALANCED_NODE;

/**
 * @brief RTL AVL Tree Root header pointing to the root MMVAD node of an EPROCESS.
 */
typedef struct _UNPD_RTL_AVL_TREE {
    PUNPD_RTL_BALANCED_NODE Root; /**< Root pointer of the process VAD AVL tree */
} UNPD_RTL_AVL_TREE, *PUNPD_RTL_AVL_TREE;

/**
 * @brief Bitfield descriptor for Virtual Address Descriptor (VAD) protection & memory attributes.
 */
typedef struct _UNPD_MMVAD_FLAGS {
    ULONG VadType : 3;        /**< Type of VAD (0 = VadNone, 1 = VadDevicePhysicalMemory, 2 = VadImageMap, etc.) */
    ULONG Protection : 5;     /**< Memory protection bits (MM_READWRITE, MM_EXECUTE_READWRITE, etc.) */
    ULONG PreferredNode : 6;  /**< NUMA node preference */
    ULONG NoChange : 1;       /**< Page protection change lock flag */
    ULONG PrivateMemory : 1;  /**< 1 if private allocation (VirtualAlloc), 0 if shared section map */
    ULONG ManagementOnly : 1; /**< Reserved management flag */
    ULONG ForCommit : 1;      /**< Deferred commitment flag */
    ULONG MemoryCost : 3;     /**< Working set memory cost rating */
} UNPD_MMVAD_FLAGS, *PUNPD_MMVAD_FLAGS;

/**
 * @brief Short form Virtual Address Descriptor structure representing contiguous virtual memory ranges in Ring-0.
 */
typedef struct _UNPD_MMVAD_SHORT {
    union {
        UNPD_RTL_BALANCED_NODE VadNode;    /**< Embedded AVL tree node for lookup positioning */
        struct _UNPD_MMVAD_SHORT* NextVad; /**< Linked list pointer fallback */
    };
    ULONG StartingVpn;           /**< Starting Virtual Page Number (Virtual Address >> 12) */
    ULONG EndingVpn;             /**< Ending Virtual Page Number (Virtual Address >> 12) */
    UCHAR StartingVpnHigh;       /**< Extended high-order bits for >44-bit address spaces */
    UCHAR EndingVpnHigh;         /**< Extended high-order bits for >44-bit address spaces */
    UCHAR CommitChargeHigh;      /**< High byte of commit charge */
    UCHAR SpareNT64Alloc;        /**< Alignment padding */
    LONG ReferenceCount;         /**< Active page reference counter */
    EX_PUSH_LOCK PushLock;       /**< Executive push lock for concurrent VAD mutations */
    union {
        ULONG LongFlags;          /**< Raw 32-bit flags value */
        UNPD_MMVAD_FLAGS VadFlags;/**< Structured bitfield flags */
    } u;
} UNPD_MMVAD_SHORT, *PUNPD_MMVAD_SHORT;

// ============================================================================
// Trap Frame & Thread Execution Contexts
// ============================================================================

/**
 * @brief System Trap Frame pushed by x64 CPU hardware & kernel stubs on interrupts/syscalls.
 */
typedef struct _UNPD_KTRAP_FRAME {
    UINT64 P1Home;
    UINT64 P2Home;
    UINT64 P3Home;
    UINT64 P4Home;
    UINT64 P5Home;
    CHAR   PreviousMode;
    UCHAR  PreviousIrql;
    UCHAR  FaultIndicator;
    UCHAR  ExceptionActive;
    ULONG  MxCsr;
    UINT64 Rax;
    UINT64 Rcx;
    UINT64 Rdx;
    UINT64 R8;
    UINT64 R9;
    UINT64 R10;
    UINT64 R11;
    UINT64 GsBase;
    UINT64 FaultAddress;
    UINT64 Rbp;
    UINT64 Rip;
    UINT64 SegCs;
    ULONG  EFlags;
    UINT64 Rsp;
    UINT64 SegSs;
} UNPD_KTRAP_FRAME, *PUNPD_KTRAP_FRAME;

// ============================================================================
// PEB, TEB & Loader Structures
// ============================================================================

typedef struct _UNPD_UNICODE_STRING_32 {
    USHORT Length;
    USHORT MaximumLength;
    ULONG  Buffer;
} UNPD_UNICODE_STRING_32, *PUNPD_UNICODE_STRING_32;

typedef struct _UNPD_PEB_LDR_DATA_GENERIC {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} UNPD_PEB_LDR_DATA_GENERIC, *PUNPD_PEB_LDR_DATA_GENERIC;

typedef struct _UNPD_LDR_DATA_TABLE_ENTRY_GENERIC {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
} UNPD_LDR_DATA_TABLE_ENTRY_GENERIC, *PUNPD_LDR_DATA_TABLE_ENTRY_GENERIC;

// ============================================================================
// Dynamic OS Build Offsets Helper
// ============================================================================

/**
 * @brief Version-aware dynamic kernel structure offsets for Windows 10 & 11 releases.
 */
struct NtBuildOffsets {
    ULONG DirectoryTableBaseOffset; /**< CR3 PML4 physical root page table offset in EPROCESS */
    ULONG VadRootOffset;            /**< VadRoot AVL tree root offset in EPROCESS */
    ULONG ActiveProcessLinksOffset; /**< ActiveProcessLinks doubly-linked list offset in EPROCESS */
    ULONG UniqueProcessIdOffset;    /**< UniqueProcessId (PID) offset in EPROCESS */
    ULONG SectionBaseAddressOffset; /**< Main executable SectionBaseAddress offset in EPROCESS */
    ULONG PebOffset;                /**< Process Environment Block (PEB) pointer offset in EPROCESS */
    ULONG ObjectTableOffset;        /**< Process Handle Table pointer offset in EPROCESS */
    ULONG ThreadListHeadOffset;     /**< ThreadListHead list header offset in EPROCESS */
    ULONG ApcStateOffset;           /**< KAPC_STATE structure offset in ETHREAD/KTHREAD */

    /**
     * @brief Resolves offsets dynamically based on target OS build.
     */
    static NtBuildOffsets GetCurrentBuildOffsets() noexcept {
        NtBuildOffsets offsets{};
        // Windows 10 (19041+) & Windows 11 (22H2 / 23H2 / 24H2) x64 Offsets
        offsets.DirectoryTableBaseOffset = 0x028;
        offsets.UniqueProcessIdOffset    = 0x440;
        offsets.ActiveProcessLinksOffset = 0x448;
        offsets.PebOffset                = 0x550;
        offsets.SectionBaseAddressOffset = 0x520;
        offsets.ObjectTableOffset        = 0x570;
        offsets.VadRootOffset            = 0x7D8;
        offsets.ThreadListHeadOffset     = 0x5e0;
        offsets.ApcStateOffset           = 0x098;
        return offsets;
    }
};

#pragma pack(pop)

} // namespace unpd::nt

#endif // UNPD_NT_NATIVE_STRUCTS_HPP
