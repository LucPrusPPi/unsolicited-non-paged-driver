#include "unpd/dispatch.hpp"
#include "unpd/kernel_raii.hpp"
#include "unpd/security.hpp"
#include "unpd/mmu/cr3_walker.hpp"
#include "unpd/exec/apc.hpp"
#include "unpd/stealth/piddb.hpp"
#include "unpd/stealth/unloaded_drivers.hpp"

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
        outBuf->TotalSwapsProcessed = devExt->MemoryManager ? devExt->MemoryManager->GetTotalSwapsCount() : 0;
        outBuf->ActiveSharedMappings = devExt->MemoryManager ? devExt->MemoryManager->GetActiveSessionsCount() : 0;
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

    if (!devExt->MemoryManager) {
        *information = 0;
        return STATUS_DEVICE_NOT_READY;
    }

    auto result = devExt->MemoryManager->AllocateMdlSharedSession(inBuf->PageCount);
    if (!result.has_value()) {
        *information = 0;
        return result.error();
    }

    const auto& sessionDesc = result.value();

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = UNPD_STATUS_SUCCESS;
    outBuf->SessionHandle = sessionDesc.SessionId;
    outBuf->UserAddress = reinterpret_cast<uint64_t>(sessionDesc.UserVa);
    outBuf->TotalBytes = static_cast<uint64_t>(sessionDesc.TotalBytes);
    outBuf->BufferCount = 2; // Double-buffering
    outBuf->BufferSize = static_cast<uint32_t>(sessionDesc.TotalBytes / 2);

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

    if (!devExt->MemoryManager) {
        *information = 0;
        return STATUS_DEVICE_NOT_READY;
    }

    NTSTATUS status = devExt->MemoryManager->FreeMdlSharedSession(inBuf->SessionHandle);

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

    if (!devExt->MemoryManager) {
        *information = 0;
        return STATUS_DEVICE_NOT_READY;
    }

    uint32_t activeIdx = 0;
    uint32_t standbyIdx = 0;
    uint64_t totalSwaps = 0;

    NTSTATUS status = devExt->MemoryManager->SwapBuffers(
        inBuf->SessionHandle,
        activeIdx,
        standbyIdx,
        totalSwaps
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

    if (!devExt->MemoryManager) {
        *information = 0;
        return STATUS_DEVICE_NOT_READY;
    }

    uint32_t blockSize = 0;
    auto result = devExt->MemoryManager->AllocateSlab(inBuf->BlockClass, blockSize);
    if (!result.has_value()) {
        *information = 0;
        return result.error();
    }

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = UNPD_STATUS_SUCCESS;
    outBuf->SlabHandle = result.value();
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

    if (inBuf->Magic != UNPD_MAGIC_RESPONSE || inBuf->SlabHandle == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    if (!devExt->MemoryManager) {
        *information = 0;
        return STATUS_DEVICE_NOT_READY;
    }

    NTSTATUS status = devExt->MemoryManager->FreeSlab(inBuf->SlabHandle, inBuf->BlockSize);

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_INVALID_PARAM;

    *information = sizeof(UNPD_FREE_RESPONSE);
    return status;
}

NTSTATUS UnpdHandleReadProcessCr3(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_CR3_MEMORY_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_CR3_MEMORY_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_CR3_MEMORY_REQUEST) || outLen < sizeof(UNPD_CR3_MEMORY_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->Cr3 == 0 || inBuf->VirtualAddress == 0 || inBuf->UserBuffer == 0 || inBuf->Size == 0 || inBuf->Size > 16 * 1024 * 1024) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T bytesRead = 0;
    NTSTATUS status = STATUS_SUCCESS;

    __try {
        ProbeForWrite(reinterpret_cast<PVOID>(inBuf->UserBuffer), static_cast<SIZE_T>(inBuf->Size), 1);
        status = unpd::mmu::Cr3Walker::ReadProcessMemoryCr3(
            inBuf->Cr3,
            inBuf->VirtualAddress,
            reinterpret_cast<PVOID>(inBuf->UserBuffer),
            static_cast<SIZE_T>(inBuf->Size),
            &bytesRead
        );
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *information = 0;
        return GetExceptionCode();
    }

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_INVALID_PARAM;
    outBuf->BytesTransferred = bytesRead;

    *information = sizeof(UNPD_CR3_MEMORY_RESPONSE);
    return status;
}

NTSTATUS UnpdHandleWriteProcessCr3(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_CR3_MEMORY_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_CR3_MEMORY_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_CR3_MEMORY_REQUEST) || outLen < sizeof(UNPD_CR3_MEMORY_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->Cr3 == 0 || inBuf->VirtualAddress == 0 || inBuf->UserBuffer == 0 || inBuf->Size == 0 || inBuf->Size > 16 * 1024 * 1024) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T bytesWritten = 0;
    NTSTATUS status = STATUS_SUCCESS;

    __try {
        ProbeForRead(reinterpret_cast<PVOID>(inBuf->UserBuffer), static_cast<SIZE_T>(inBuf->Size), 1);
        status = unpd::mmu::Cr3Walker::WriteProcessMemoryCr3(
            inBuf->Cr3,
            inBuf->VirtualAddress,
            reinterpret_cast<const void*>(inBuf->UserBuffer),
            static_cast<SIZE_T>(inBuf->Size),
            &bytesWritten
        );
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *information = 0;
        return GetExceptionCode();
    }

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_INVALID_PARAM;
    outBuf->BytesTransferred = bytesWritten;

    *information = sizeof(UNPD_CR3_MEMORY_RESPONSE);
    return status;
}

NTSTATUS UnpdHandleQueueApc(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_APC_QUEUE_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_APC_QUEUE_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_APC_QUEUE_REQUEST) || outLen < sizeof(UNPD_APC_QUEUE_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->TargetThreadId == 0 || inBuf->UserRoutine == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = unpd::exec::KernelApc::QueueUserApc(
        reinterpret_cast<HANDLE>(static_cast<uintptr_t>(inBuf->TargetThreadId)),
        reinterpret_cast<PVOID>(inBuf->UserRoutine),
        reinterpret_cast<PVOID>(inBuf->UserContext)
    );

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_INVALID_PARAM;

    *information = sizeof(UNPD_APC_QUEUE_RESPONSE);
    return status;
}

NTSTATUS UnpdHandleCleanPiDdb(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_STEALTH_PIDDB_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_STEALTH_PIDDB_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_STEALTH_PIDDB_REQUEST) || outLen < sizeof(UNPD_STEALTH_PIDDB_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->DriverName[0] == L'\0') {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    inBuf->DriverName[63] = L'\0';
    UNICODE_STRING driverName{};
    RtlInitUnicodeString(&driverName, inBuf->DriverName);

    NTSTATUS status = unpd::stealth::PiDdbCleaner::CleanDriverTrace(&driverName, inBuf->TimeDateStamp);

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_INVALID_PARAM;

    *information = sizeof(UNPD_STEALTH_PIDDB_RESPONSE);
    return status;
}

NTSTATUS UnpdHandleCleanUnloaded(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_STEALTH_UNLOADED_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_STEALTH_UNLOADED_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_STEALTH_UNLOADED_REQUEST) || outLen < sizeof(UNPD_STEALTH_UNLOADED_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = STATUS_SUCCESS;

    if (inBuf->DriverName[0] != L'\0') {
        inBuf->DriverName[63] = L'\0';
        UNICODE_STRING driverName{};
        RtlInitUnicodeString(&driverName, inBuf->DriverName);
        NTSTATUS subStatus = unpd::stealth::UnloadedCleaner::CleanUnloadedDrivers(&driverName);
        if (!NT_SUCCESS(subStatus)) {
            status = subStatus;
        }
    }

    if (inBuf->BigPoolAddress != 0) {
        NTSTATUS subStatus = unpd::stealth::UnloadedCleaner::CleanBigPoolTable(reinterpret_cast<PVOID>(inBuf->BigPoolAddress));
        if (!NT_SUCCESS(subStatus)) {
            status = subStatus;
        }
    }

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_INVALID_PARAM;

    *information = sizeof(UNPD_STEALTH_UNLOADED_RESPONSE);
    return status;
}

#include "unpd/simd/simd_engine.hpp"
#include "unpd/mmu/vmt_resolver.hpp"

NTSTATUS UnpdHandleSimdPatternScan(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_SIMD_SCAN_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_SIMD_SCAN_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_SIMD_SCAN_REQUEST) || outLen < sizeof(UNPD_SIMD_SCAN_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->BaseAddress == 0 || inBuf->BufferSize == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    // Validate mask is null-terminated within buffer
    inBuf->Mask[63] = '\0';
    inBuf->Pattern[63] = 0;

    const void* match = nullptr;
    __try {
        if (inBuf->BaseAddress < 0x7FFFFFFFFFFFULL) {
            // User mode address range: Probe memory thoroughly
            ProbeForRead(reinterpret_cast<PVOID>(inBuf->BaseAddress), inBuf->BufferSize, 1);
        } else {
#ifdef _KERNEL_MODE
            // Kernel address range: Validate every page in range is valid and resident to prevent BugCheck 0x50
            const uintptr_t startPage = inBuf->BaseAddress & ~0xFFFULL;
            const uintptr_t endPage = (inBuf->BaseAddress + inBuf->BufferSize - 1) & ~0xFFFULL;
            for (uintptr_t page = startPage; page <= endPage; page += 0x1000) {
                if (!MmIsAddressValid(reinterpret_cast<PVOID>(page))) {
                    *information = 0;
                    return STATUS_ACCESS_VIOLATION;
                }
            }
#endif
        }

        match = unpd::simd::SimdEngine::ScanPattern(
            reinterpret_cast<const void*>(inBuf->BaseAddress),
            inBuf->BufferSize,
            inBuf->Pattern,
            inBuf->Mask
        );
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *information = 0;
        return GetExceptionCode();
    }

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = (match != nullptr) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_NOT_FOUND;
    outBuf->MatchAddress = reinterpret_cast<uint64_t>(match);

    *information = sizeof(UNPD_SIMD_SCAN_RESPONSE);
    return (match != nullptr) ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

NTSTATUS UnpdHandleResolveVmt(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_RESOLVE_VMT_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_RESOLVE_VMT_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_RESOLVE_VMT_REQUEST) || outLen < sizeof(UNPD_RESOLVE_VMT_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->ModuleBase == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    unpd::mmu::VmtResolver::VmtInfo info{};
    bool found = unpd::mmu::VmtResolver::ResolveVtable(
        reinterpret_cast<const void*>(inBuf->ModuleBase),
        inBuf->ModuleSize,
        inBuf->CodeSectionStart,
        inBuf->CodeSectionSize,
        info
    );

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = found ? UNPD_STATUS_SUCCESS : UNPD_STATUS_NOT_FOUND;
    outBuf->VtableAddress = info.VtableAddress;
    outBuf->MethodCount = info.MethodCount;
    outBuf->FirstMethodAddress = info.FirstMethodAddress;

    *information = sizeof(UNPD_RESOLVE_VMT_RESPONSE);
    return found ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

#include "unpd/input/mouse.hpp"
#include "unpd/exec/process_info.hpp"

NTSTATUS UnpdHandleMoveMouseRelative(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_MOUSE_MOVE_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_MOUSE_MOVE_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_MOUSE_MOVE_REQUEST) || outLen < sizeof(UNPD_MOUSE_MOVE_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = unpd::input::MouseEngine::InjectRelativeMovement(inBuf->DeltaX, inBuf->DeltaY, inBuf->ButtonFlags);

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_INVALID_PARAM;

    *information = sizeof(UNPD_MOUSE_MOVE_RESPONSE);
    return status;
}

NTSTATUS UnpdHandleQueryProcessBase(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    UNREFERENCED_PARAMETER(devExt);

    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_PROCESS_BASE_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_PROCESS_BASE_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_PROCESS_BASE_REQUEST) || outLen < sizeof(UNPD_PROCESS_BASE_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->ProcessId == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    unpd::exec::ProcessInfo info{};
    NTSTATUS status = unpd::exec::ProcessInfoEngine::QueryProcessInfo(inBuf->ProcessId, info);

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = NT_SUCCESS(status) ? UNPD_STATUS_SUCCESS : UNPD_STATUS_NOT_FOUND;
    outBuf->BaseAddress = info.SectionBaseAddress;
    outBuf->PebAddress = info.PebAddress;

    *information = sizeof(UNPD_PROCESS_BASE_RESPONSE);
    return status;
}
