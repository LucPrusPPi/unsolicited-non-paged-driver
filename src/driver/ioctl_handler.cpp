#include "unpd/dispatch.hpp"
#include "unpd/kernel_raii.hpp"
#include "unpd/security.hpp"

NTSTATUS UnpdHandlePing(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);
    UNREFERENCED_PARAMETER(irp);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_PING_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_PING_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_PING_REQUEST) || outLen < sizeof(UNPD_PING_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    LARGE_INTEGER systemTime;
    KeQuerySystemTimePrecise(&systemTime);

    uint32_t seq = inBuf->Sequence;

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Sequence = seq + 1;
    outBuf->KernelTimestamp = static_cast<uint64_t>(systemTime.QuadPart);
    outBuf->DriverVersionMajor = 1;
    outBuf->DriverVersionMinor = 0;

    *information = sizeof(UNPD_PING_RESPONSE);
    return STATUS_SUCCESS;
}

NTSTATUS UnpdHandleQueryStats(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* outBuf = static_cast<PUNPD_STATS_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (outLen < sizeof(UNPD_STATS_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    {
        unpd::SpinlockGuard guard(&devExt->StateLock);

        outBuf->Magic = UNPD_MAGIC_RESPONSE;
        outBuf->ActiveAllocations = devExt->ActiveAllocations;
        outBuf->TotalBytesAllocated = devExt->TotalBytesAllocated;
        outBuf->TotalBytesFreed = devExt->TotalBytesFreed;
        outBuf->TotalIoctlProcessed = devExt->TotalIoctlProcessed;
        outBuf->SpinLockContentionCount = devExt->SpinLockContentionCount;
        outBuf->TotalSwapsProcessed = devExt->PageEngine.TotalSwaps;
        outBuf->ActiveSharedMappings = devExt->PageEngine.ActiveSessions;
    }

    *information = sizeof(UNPD_STATS_RESPONSE);
    return STATUS_SUCCESS;
}

NTSTATUS UnpdHandleProcessBufferDirect(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (inLen < sizeof(UNPD_BUFFER_HEADER) || outLen < sizeof(UNPD_BUFFER_HEADER)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (irp->MdlAddress == nullptr) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    PVOID buffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority | MdlMappingNoExecute);
    if (buffer == nullptr) {
        *information = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    auto* header = static_cast<PUNPD_BUFFER_HEADER>(buffer);
    if (header->Magic != UNPD_MAGIC_REQUEST) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    uint32_t checksum = 0;
    auto* bytePtr = static_cast<uint8_t*>(buffer) + sizeof(UNPD_BUFFER_HEADER);
    ULONG payloadLen = outLen - sizeof(UNPD_BUFFER_HEADER);

    for (ULONG i = 0; i < payloadLen; ++i) {
        checksum = (checksum * 31) + bytePtr[i];
    }

    header->Magic = UNPD_MAGIC_RESPONSE;
    header->Checksum = checksum;

    *information = outLen;
    return STATUS_SUCCESS;
}

NTSTATUS UnpdHandleProcessBufferNeither(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);
    UNREFERENCED_PARAMETER(irp);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID inBuf = irpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    PVOID outBuf = irp->UserBuffer;

    if (inLen < sizeof(UNPD_BUFFER_HEADER) || outLen < sizeof(UNPD_BUFFER_HEADER)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    NTSTATUS status = unpd::ProbeUserBufferForRead(inBuf, inLen, alignof(uint32_t));
    if (!NT_SUCCESS(status)) {
        *information = 0;
        return status;
    }

    status = unpd::ProbeUserBufferForWrite(outBuf, outLen, alignof(uint32_t));
    if (!NT_SUCCESS(status)) {
        *information = 0;
        return status;
    }

    __try {
        auto* inHeader = static_cast<PUNPD_BUFFER_HEADER>(inBuf);
        auto* outHeader = static_cast<PUNPD_BUFFER_HEADER>(outBuf);

        if (inHeader->Magic != UNPD_MAGIC_REQUEST) {
            *information = 0;
            return STATUS_INVALID_PARAMETER;
        }

        outHeader->Magic = UNPD_MAGIC_RESPONSE;
        outHeader->DataLength = inHeader->DataLength;
        outHeader->Operation = inHeader->Operation;
        outHeader->Checksum = 0xAA55AA55;

        *information = sizeof(UNPD_BUFFER_HEADER);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        *information = 0;
        return GetExceptionCode();
    }

    return STATUS_SUCCESS;
}

NTSTATUS UnpdHandleMapSharedMemory(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_MAP_SHARED_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_MAP_SHARED_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_MAP_SHARED_REQUEST) || outLen < sizeof(UNPD_MAP_SHARED_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->PageCount == 0 || inBuf->PageCount > 256) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    uint64_t handle = 0;
    PVOID userVa = nullptr;
    SIZE_T totalBytes = 0;

    NTSTATUS status = UnpdCreateSharedSession(
        &devExt->PageEngine,
        inBuf->PageCount,
        &handle,
        &userVa,
        &totalBytes
    );

    if (!NT_SUCCESS(status)) {
        *information = 0;
        return status;
    }

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = UNPD_STATUS_SUCCESS;
    outBuf->SessionHandle = handle;
    outBuf->UserAddress = reinterpret_cast<uint64_t>(userVa);
    outBuf->TotalBytes = static_cast<uint64_t>(totalBytes);
    outBuf->BufferCount = 2; // Double-buffering
    outBuf->BufferSize = static_cast<uint32_t>(totalBytes / 2);

    *information = sizeof(UNPD_MAP_SHARED_RESPONSE);
    return STATUS_SUCCESS;
}

NTSTATUS UnpdHandleUnmapSharedMemory(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_UNMAP_SHARED_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_UNMAP_SHARED_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_UNMAP_SHARED_REQUEST) || outLen < sizeof(UNPD_UNMAP_SHARED_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->SessionHandle == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = UnpdDestroySharedSession(&devExt->PageEngine, inBuf->SessionHandle);

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_NOT_FOUND;

    *information = sizeof(UNPD_UNMAP_SHARED_RESPONSE);
    return status;
}

NTSTATUS UnpdHandleSwapBuffers(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_SWAP_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_SWAP_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_SWAP_REQUEST) || outLen < sizeof(UNPD_SWAP_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->SessionHandle == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    uint32_t activeIdx = 0;
    uint32_t standbyIdx = 0;
    uint64_t totalSwaps = 0;

    NTSTATUS status = UnpdSwapSessionBuffers(
        &devExt->PageEngine,
        inBuf->SessionHandle,
        &activeIdx,
        &standbyIdx,
        &totalSwaps
    );

    if (!NT_SUCCESS(status)) {
        *information = 0;
        return status;
    }

    LARGE_INTEGER systemTime;
    KeQuerySystemTimePrecise(&systemTime);

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = UNPD_STATUS_SUCCESS;
    outBuf->ActiveBufferIndex = activeIdx;
    outBuf->StandbyBufferIndex = standbyIdx;
    outBuf->SwapTimestamp = static_cast<uint64_t>(systemTime.QuadPart);
    outBuf->TotalSwaps = totalSwaps;

    *information = sizeof(UNPD_SWAP_RESPONSE);
    return STATUS_SUCCESS;
}

NTSTATUS UnpdHandleSlabAlloc(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_SLAB_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_SLAB_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_SLAB_REQUEST) || outLen < sizeof(UNPD_SLAB_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->BlockClass >= 4) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    PVOID blockAddress = nullptr;
    uint32_t blockSize = 0;

    NTSTATUS status = UnpdSlabAllocate(
        &devExt->PageEngine,
        inBuf->BlockClass,
        &blockAddress,
        &blockSize
    );

    if (!NT_SUCCESS(status)) {
        *information = 0;
        return status;
    }

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = UNPD_STATUS_SUCCESS;
    outBuf->SlabHandle = reinterpret_cast<uint64_t>(blockAddress);
    outBuf->BlockSize = blockSize;

    *information = sizeof(UNPD_SLAB_RESPONSE);
    return STATUS_SUCCESS;
}

NTSTATUS UnpdHandleSlabFree(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_SLAB_RESPONSE>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_FREE_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_SLAB_RESPONSE) || outLen < sizeof(UNPD_FREE_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->SlabHandle == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    uint32_t blockClass = 0;
    if (inBuf->BlockSize == 64) blockClass = 0;
    else if (inBuf->BlockSize == 256) blockClass = 1;
    else if (inBuf->BlockSize == 1024) blockClass = 2;
    else if (inBuf->BlockSize == 4096) blockClass = 3;
    else {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = UnpdSlabFree(
        &devExt->PageEngine,
        blockClass,
        reinterpret_cast<PVOID>(inBuf->SlabHandle)
    );

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_NOT_FOUND;

    *information = sizeof(UNPD_FREE_RESPONSE);
    return status;
}
