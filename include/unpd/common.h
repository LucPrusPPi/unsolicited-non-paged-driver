#pragma once

#ifndef UNPD_COMMON_H
#define UNPD_COMMON_H

#ifdef _KERNEL_MODE
#include <ntdef.h>
#include <ntstatus.h>
#else
#include <windows.h>
#include <winioctl.h>
#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif
#ifndef UNICODE_STRING
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;
#endif
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#endif
#ifndef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES    ((NTSTATUS)0xC000009AL)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#endif
#ifndef STATUS_NOT_SUPPORTED
#define STATUS_NOT_SUPPORTED             ((NTSTATUS)0xC00000BBB)
#endif
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Driver Identification & Device Names
// ============================================================================
#define UNPD_DEVICE_NAME_W          L"\\Device\\UnsolicitedNonPagedDriver"
#define UNPD_DOS_DEVICE_NAME_W      L"\\DosDevices\\UnsolicitedNonPagedDriver"
#define UNPD_USERMODE_PATH_W        L"\\\\.\\UnsolicitedNonPagedDriver"

#define UNPD_DEVICE_TYPE            0x8000u
#define UNPD_POOL_TAG               'DPNU'  // 'UNPD' in little-endian
#define UNPD_PAGE_TAG               'GAPU'  // 'UPAG' in little-endian
#define UNPD_SLAB_TAG               'BALS'  // 'SLAB' in little-endian

// ============================================================================
// IOCTL Function Codes (Base: 0x800)
// ============================================================================
#define UNPD_IOCTL_INDEX_PING                   0x800
#define UNPD_IOCTL_INDEX_ALLOCATE_NONPAGED      0x801
#define UNPD_IOCTL_INDEX_FREE_NONPAGED          0x802
#define UNPD_IOCTL_INDEX_QUERY_STATS            0x803
#define UNPD_IOCTL_INDEX_PROCESS_BUFFER_DIRECT  0x804
#define UNPD_IOCTL_INDEX_PROCESS_BUFFER_NEITHER 0x805
#define UNPD_IOCTL_INDEX_MAP_SHARED_MEMORY      0x806
#define UNPD_IOCTL_INDEX_UNMAP_SHARED_MEMORY    0x807
#define UNPD_IOCTL_INDEX_SWAP_BUFFERS           0x808
#define UNPD_IOCTL_INDEX_SLAB_ALLOC             0x809
#define UNPD_IOCTL_INDEX_SLAB_FREE              0x80A

// IOCTL Definitions using CTL_CODE macro
#define IOCTL_UNPD_PING \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_PING, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UNPD_ALLOCATE_NONPAGED \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_ALLOCATE_NONPAGED, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UNPD_FREE_NONPAGED \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_FREE_NONPAGED, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UNPD_QUERY_STATS \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_QUERY_STATS, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UNPD_PROCESS_BUFFER_DIRECT \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_PROCESS_BUFFER_DIRECT, METHOD_IN_DIRECT, FILE_READ_DATA | FILE_WRITE_DATA)

#define IOCTL_UNPD_PROCESS_BUFFER_NEITHER \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_PROCESS_BUFFER_NEITHER, METHOD_NEITHER, FILE_READ_DATA | FILE_WRITE_DATA)

#define IOCTL_UNPD_MAP_SHARED_MEMORY \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_MAP_SHARED_MEMORY, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UNPD_UNMAP_SHARED_MEMORY \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_UNMAP_SHARED_MEMORY, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UNPD_SWAP_BUFFERS \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_SWAP_BUFFERS, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UNPD_SLAB_ALLOC \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_SLAB_ALLOC, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UNPD_SLAB_FREE \
    CTL_CODE(UNPD_DEVICE_TYPE, UNPD_IOCTL_INDEX_SLAB_FREE, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ============================================================================
// Magic Validation Constants
// ============================================================================
#define UNPD_MAGIC_REQUEST          0x554E5044  // 'UNPD'
#define UNPD_MAGIC_RESPONSE         0x44504E55  // 'DPNU'

#define UNPD_STATUS_SUCCESS         0x00000000
#define UNPD_STATUS_INVALID_MAGIC   0xC0000001
#define UNPD_STATUS_INVALID_PARAM   0xC000000D
#define UNPD_STATUS_NO_MEMORY       0xC0000017
#define UNPD_STATUS_NOT_FOUND       0xC0000225

// ============================================================================
// Protocol Structures
// ============================================================================

#pragma pack(push, 8)

typedef struct _UNPD_BUFFER_HEADER {
    uint32_t Magic;
    uint32_t Operation;
    uint32_t DataLength;
    uint32_t Checksum;
} UNPD_BUFFER_HEADER, *PUNPD_BUFFER_HEADER;

typedef struct _UNPD_PING_REQUEST {
    uint32_t Magic;
    uint32_t Sequence;
    uint64_t Timestamp;
} UNPD_PING_REQUEST, *PUNPD_PING_REQUEST;

typedef struct _UNPD_PING_RESPONSE {
    uint32_t Magic;
    uint32_t Sequence;
    uint64_t KernelTimestamp;
    uint32_t DriverVersionMajor;
    uint32_t DriverVersionMinor;
} UNPD_PING_RESPONSE, *PUNPD_PING_RESPONSE;

typedef struct _UNPD_ALLOC_REQUEST {
    uint32_t Magic;
    uint32_t Flags;
    uint64_t ByteCount;
    uint64_t Reserved;
} UNPD_ALLOC_REQUEST, *PUNPD_ALLOC_REQUEST;

typedef struct _UNPD_ALLOC_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
    uint64_t AllocatedHandle;
    uint64_t AllocatedSize;
} UNPD_ALLOC_RESPONSE, *PUNPD_ALLOC_RESPONSE;

typedef struct _UNPD_FREE_REQUEST {
    uint32_t Magic;
    uint32_t Reserved;
    uint64_t AllocatedHandle;
} UNPD_FREE_REQUEST, *PUNPD_FREE_REQUEST;

typedef struct _UNPD_FREE_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
    uint64_t FreedByteCount;
} UNPD_FREE_RESPONSE, *PUNPD_FREE_RESPONSE;

typedef struct _UNPD_STATS_RESPONSE {
    uint32_t Magic;
    uint32_t ActiveAllocations;
    uint64_t TotalBytesAllocated;
    uint64_t TotalBytesFreed;
    uint64_t TotalIoctlProcessed;
    uint64_t SpinLockContentionCount;
    uint64_t TotalSwapsProcessed;
    uint32_t ActiveSharedMappings;
    uint32_t Reserved;
} UNPD_STATS_RESPONSE, *PUNPD_STATS_RESPONSE;

typedef struct _UNPD_MAP_SHARED_REQUEST {
    uint32_t Magic;
    uint32_t PageCount;
} UNPD_MAP_SHARED_REQUEST, *PUNPD_MAP_SHARED_REQUEST;

typedef struct _UNPD_MAP_SHARED_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
    uint64_t SessionHandle;
    uint64_t UserAddress;
    uint64_t TotalBytes;
    uint32_t BufferSize;
    uint32_t BufferCount;
} UNPD_MAP_SHARED_RESPONSE, *PUNPD_MAP_SHARED_RESPONSE;

typedef struct _UNPD_UNMAP_SHARED_REQUEST {
    uint32_t Magic;
    uint32_t Reserved;
    uint64_t SessionHandle;
} UNPD_UNMAP_SHARED_REQUEST, *PUNPD_UNMAP_SHARED_REQUEST;

typedef struct _UNPD_UNMAP_SHARED_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
} UNPD_UNMAP_SHARED_RESPONSE, *PUNPD_UNMAP_SHARED_RESPONSE;

typedef struct _UNPD_SWAP_REQUEST {
    uint32_t Magic;
    uint32_t Reserved;
    uint64_t SessionHandle;
} UNPD_SWAP_REQUEST, *PUNPD_SWAP_REQUEST;

typedef struct _UNPD_SWAP_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
    uint32_t ActiveBufferIndex;
    uint32_t StandbyBufferIndex;
    uint64_t TotalSwaps;
    uint64_t SwapTimestamp;
} UNPD_SWAP_RESPONSE, *PUNPD_SWAP_RESPONSE;

typedef struct _UNPD_SLAB_REQUEST {
    uint32_t Magic;
    uint32_t BlockClass;
} UNPD_SLAB_REQUEST, *PUNPD_SLAB_REQUEST;

typedef struct _UNPD_SLAB_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
    uint64_t SlabHandle;
    uint32_t BlockSize;
    uint32_t Reserved;
} UNPD_SLAB_RESPONSE, *PUNPD_SLAB_RESPONSE;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // UNPD_COMMON_H
