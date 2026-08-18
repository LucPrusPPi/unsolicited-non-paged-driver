#pragma once

#ifndef UNPD_COMMON_H
#define UNPD_COMMON_H

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#include <cstdint>
#include <cstddef>
#endif

#ifdef _KERNEL_MODE
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned long      uint32_t;
typedef unsigned long long uint64_t;
#endif

#define UNPD_DEVICE_TYPE            0x8000u
#define UNPD_DEVICE_NAME_W          L"\\Device\\UnsolicitedNonPagedDriver"
#define UNPD_DOS_DEVICE_NAME_W      L"\\DosDevices\\UnsolicitedNonPagedDriver"
#define UNPD_USERMODE_PATH_W        L"\\\\.\\UnsolicitedNonPagedDriver"

#define UNPD_POOL_TAG               'DPNU' // 'UNPD'
#define UNPD_PAGE_TAG               'PGNU' // 'UNPG'
#define UNPD_SLAB_TAG               'SLNU' // 'UNSL'

#define IOCTL_UNPD_PING \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x800u, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_ALLOCATE_NONPAGED \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x801u, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_FREE_NONPAGED \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x802u, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_QUERY_STATS \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x803u, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_UNPD_PROCESS_BUFFER_DIRECT \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x804u, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_PROCESS_BUFFER_NEITHER \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x805u, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_MAP_SHARED_MEMORY \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x806u, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_UNMAP_SHARED_MEMORY \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x807u, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_SWAP_BUFFERS \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x808u, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_SLAB_ALLOC \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x809u, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_UNPD_SLAB_FREE \
    CTL_CODE(UNPD_DEVICE_TYPE, 0x80A, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define UNPD_MAGIC_REQUEST          0x554E5044
#define UNPD_MAGIC_RESPONSE         0x44504E55

#define UNPD_SHARED_PAGE_COUNT      16 // 64 KB total shared memory per session
#define UNPD_PAGE_SIZE              4096

#pragma pack(push, 8)

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
} UNPD_ALLOC_REQUEST, *PUNPD_ALLOC_REQUEST;

typedef struct _UNPD_ALLOC_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
    uint64_t AllocatedHandle;
    uint64_t AllocatedSize;
} UNPD_ALLOC_RESPONSE, *PUNPD_ALLOC_RESPONSE;

typedef struct _UNPD_FREE_REQUEST {
    uint32_t Magic;
    uint64_t AllocatedHandle;
} UNPD_FREE_REQUEST, *PUNPD_FREE_REQUEST;

typedef struct _UNPD_FREE_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
} UNPD_FREE_RESPONSE, *PUNPD_FREE_RESPONSE;

typedef struct _UNPD_STATS_RESPONSE {
    uint32_t Magic;
    uint32_t ActiveAllocations;
    uint64_t TotalBytesAllocated;
    uint64_t TotalBytesFreed;
    uint64_t TotalIoctlProcessed;
    uint64_t SpinLockContentionCount;
    uint64_t TotalSwapsProcessed;
    uint64_t ActiveSharedMappings;
} UNPD_STATS_RESPONSE, *PUNPD_STATS_RESPONSE;

typedef struct _UNPD_BUFFER_HEADER {
    uint32_t Magic;
    uint32_t DataLength;
    uint32_t Checksum;
    uint32_t Operation;
} UNPD_BUFFER_HEADER, *PUNPD_BUFFER_HEADER;

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
    uint32_t BufferCount;
    uint32_t BufferSize;
} UNPD_MAP_SHARED_RESPONSE, *PUNPD_MAP_SHARED_RESPONSE;

typedef struct _UNPD_UNMAP_SHARED_REQUEST {
    uint32_t Magic;
    uint64_t SessionHandle;
} UNPD_UNMAP_SHARED_REQUEST, *PUNPD_UNMAP_SHARED_REQUEST;

typedef struct _UNPD_UNMAP_SHARED_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
} UNPD_UNMAP_SHARED_RESPONSE, *PUNPD_UNMAP_SHARED_RESPONSE;

typedef struct _UNPD_SWAP_REQUEST {
    uint32_t Magic;
    uint64_t SessionHandle;
    uint32_t UpstreamSequence;
    uint32_t PayloadSize;
} UNPD_SWAP_REQUEST, *PUNPD_SWAP_REQUEST;

typedef struct _UNPD_SWAP_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
    uint32_t ActiveBufferIndex;
    uint32_t StandbyBufferIndex;
    uint64_t SwapTimestamp;
    uint64_t TotalSwaps;
} UNPD_SWAP_RESPONSE, *PUNPD_SWAP_RESPONSE;

typedef struct _UNPD_SLAB_REQUEST {
    uint32_t Magic;
    uint32_t BlockClass; // 0=64B, 1=256B, 2=1024B, 3=4096B
} UNPD_SLAB_REQUEST, *PUNPD_SLAB_REQUEST;

typedef struct _UNPD_SLAB_RESPONSE {
    uint32_t Magic;
    uint32_t Status;
    uint64_t SlabHandle;
    uint32_t BlockSize;
} UNPD_SLAB_RESPONSE, *PUNPD_SLAB_RESPONSE;

#pragma pack(pop)

#define UNPD_STATUS_SUCCESS                 0x00000000
#define UNPD_STATUS_INVALID_MAGIC           0xC0000001
#define UNPD_STATUS_INVALID_BUFFER_SIZE     0xC0000004
#define UNPD_STATUS_INSUFFICIENT_RESOURCES  0xC000009A
#define UNPD_STATUS_NOT_FOUND               0xC0000225
#define UNPD_STATUS_ALREADY_MAPPED          0xC00000A2

#endif // UNPD_COMMON_H
