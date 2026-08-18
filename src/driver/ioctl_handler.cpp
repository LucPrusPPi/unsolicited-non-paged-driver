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
