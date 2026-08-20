#include "unpd/dispatch.hpp"

extern "C"
NTSTATUS
UnpdCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
) {
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
UnpdDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
) {
    auto* devExt = static_cast<PUNPD_DEVICE_EXTENSION>(DeviceObject->DeviceExtension);
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR information = 0;

    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&devExt->TotalIoctlProcessed));

    switch (controlCode) {
    case IOCTL_UNPD_PING:
        status = UnpdHandlePing(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_ALLOCATE_NONPAGED:
        status = UnpdHandleAllocate(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_FREE_NONPAGED:
        status = UnpdHandleFree(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_QUERY_STATS:
        status = UnpdHandleQueryStats(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_PROCESS_BUFFER_DIRECT:
        status = UnpdHandleProcessBufferDirect(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_PROCESS_BUFFER_NEITHER:
        status = UnpdHandleProcessBufferNeither(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_MAP_SHARED_MEMORY:
        status = UnpdHandleMapSharedMemory(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_UNMAP_SHARED_MEMORY:
        status = UnpdHandleUnmapSharedMemory(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_SWAP_BUFFERS:
        status = UnpdHandleSwapBuffers(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_SLAB_ALLOC:
        status = UnpdHandleSlabAlloc(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_SLAB_FREE:
        status = UnpdHandleSlabFree(devExt, Irp, irpSp, &information);
        break;

#if UNPD_FEATURE_CR3_PML4_OPERATIONS
    case IOCTL_UNPD_READ_PROCESS_CR3:
        status = UnpdHandleReadProcessCr3(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_WRITE_PROCESS_CR3:
        status = UnpdHandleWriteProcessCr3(devExt, Irp, irpSp, &information);
        break;
#endif

#if UNPD_FEATURE_KERNEL_APC_INJECTION
    case IOCTL_UNPD_QUEUE_KAPC:
        status = UnpdHandleQueueApc(devExt, Irp, irpSp, &information);
        break;
#endif

#if UNPD_FEATURE_STEALTH_CLEANERS
    case IOCTL_UNPD_CLEAN_PIDDB:
        status = UnpdHandleCleanPiDdb(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_CLEAN_UNLOADED:
        status = UnpdHandleCleanUnloaded(devExt, Irp, irpSp, &information);
        break;
#endif

    case IOCTL_UNPD_SIMD_PATTERN_SCAN:
        status = UnpdHandleSimdPatternScan(devExt, Irp, irpSp, &information);
        break;

    case IOCTL_UNPD_RESOLVE_VMT:
        status = UnpdHandleResolveVmt(devExt, Irp, irpSp, &information);
        break;

#if UNPD_FEATURE_SYNTHETIC_MOUSE_INPUT
    case IOCTL_UNPD_MOVE_MOUSE_RELATIVE:
        status = UnpdHandleMoveMouseRelative(devExt, Irp, irpSp, &information);
        break;
#endif

#if UNPD_FEATURE_PROCESS_BASE_QUERY
    case IOCTL_UNPD_QUERY_PROCESS_BASE:
        status = UnpdHandleQueryProcessBase(devExt, Irp, irpSp, &information);
        break;
#endif

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        information = 0;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}
