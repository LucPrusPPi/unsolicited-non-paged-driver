#pragma once

#ifndef UNPD_PAGE_ENGINE_HPP
#define UNPD_PAGE_ENGINE_HPP

#ifdef _KERNEL_MODE

#include <ntddk.h>
#include "common.h"

typedef struct _UNPD_SHARED_SESSION {
    LIST_ENTRY ListEntry;
    uint64_t SessionHandle;
    PMDL Mdl;
    PVOID KernelVa;
    PVOID UserVa;
    PEPROCESS Process;
    SIZE_T ByteCount;
    uint32_t PageCount;
    volatile LONG ActiveBufferIndex; // 0 or 1 for double buffering
    uint64_t TotalSwaps;
    KSPIN_LOCK SessionLock;
} UNPD_SHARED_SESSION, *PUNPD_SHARED_SESSION;

typedef struct _UNPD_SLAB_BLOCK {
    struct _UNPD_SLAB_BLOCK* Next;
} UNPD_SLAB_BLOCK, *PUNPD_SLAB_BLOCK;

typedef struct _UNPD_SLAB_CACHE {
    uint32_t BlockSize;
    uint32_t TotalBlocks;
    uint32_t FreeBlocks;
    PUNPD_SLAB_BLOCK FreeListHead;
    KSPIN_LOCK CacheLock;
    PVOID PageBacking;
    PMDL PageMdl;
} UNPD_SLAB_CACHE, *PUNPD_SLAB_CACHE;

typedef struct _UNPD_PAGE_ENGINE {
    KSPIN_LOCK EngineLock;
    LIST_ENTRY SessionListHead;
    uint32_t ActiveSessions;
    uint64_t NextSessionHandle;
    uint64_t TotalSwaps;
    UNPD_SLAB_CACHE Slabs[4]; // 64B, 256B, 1024B, 4096B
} UNPD_PAGE_ENGINE, *PUNPD_PAGE_ENGINE;

NTSTATUS UnpdInitPageEngine(PUNPD_PAGE_ENGINE engine);
VOID UnpdCleanupPageEngine(PUNPD_PAGE_ENGINE engine);

NTSTATUS UnpdCreateSharedSession(
    PUNPD_PAGE_ENGINE engine,
    uint32_t pageCount,
    uint64_t* outSessionHandle,
    PVOID* outUserVa,
    SIZE_T* outTotalBytes
);

NTSTATUS UnpdDestroySharedSession(
    PUNPD_PAGE_ENGINE engine,
    uint64_t sessionHandle
);

NTSTATUS UnpdSwapSessionBuffers(
    PUNPD_PAGE_ENGINE engine,
    uint64_t sessionHandle,
    uint32_t* outActiveIndex,
    uint32_t* outStandbyIndex,
    uint64_t* outTotalSwaps
);

NTSTATUS UnpdSlabAllocate(
    PUNPD_PAGE_ENGINE engine,
    uint32_t blockClass,
    PVOID* outAddress,
    uint32_t* outBlockSize
);

NTSTATUS UnpdSlabFree(
    PUNPD_PAGE_ENGINE engine,
    uint32_t blockClass,
    PVOID address
);

#endif // _KERNEL_MODE
#endif // UNPD_PAGE_ENGINE_HPP
