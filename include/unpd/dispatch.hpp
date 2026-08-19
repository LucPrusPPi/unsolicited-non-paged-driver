#pragma once

#ifndef UNPD_DISPATCH_HPP
#define UNPD_DISPATCH_HPP

#ifdef _KERNEL_MODE

#include <ntddk.h>
#include "common.h"
#include "page_engine.hpp"

typedef struct _UNPD_ALLOCATION_ENTRY {
    LIST_ENTRY ListEntry;
    uint64_t Handle;
    PVOID Address;
    SIZE_T Size;
    ULONG Tag;
} UNPD_ALLOCATION_ENTRY, *PUNPD_ALLOCATION_ENTRY;

typedef struct _UNPD_DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING SymbolicLinkName;
    KSPIN_LOCK StateLock;
    LIST_ENTRY AllocationListHead;
    uint32_t ActiveAllocations;
    uint64_t NextAllocationHandle;
    uint64_t TotalBytesAllocated;
    uint64_t TotalBytesFreed;
    uint64_t TotalIoctlProcessed;
    uint64_t SpinLockContentionCount;
    UNPD_PAGE_ENGINE PageEngine;
} UNPD_DEVICE_EXTENSION, *PUNPD_DEVICE_EXTENSION;

extern "C" {
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;

_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH UnpdCreateClose;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH UnpdDeviceControl;
}

NTSTATUS UnpdHandlePing(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleAllocate(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleFree(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleQueryStats(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleProcessBufferDirect(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleProcessBufferNeither(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleMapSharedMemory(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleUnmapSharedMemory(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleSwapBuffers(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleSlabAlloc(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleSlabFree(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleReadProcessCr3(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleWriteProcessCr3(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleQueueApc(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleCleanPiDdb(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

NTSTATUS UnpdHandleCleanUnloaded(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
);

#endif // _KERNEL_MODE
#endif // UNPD_DISPATCH_HPP
