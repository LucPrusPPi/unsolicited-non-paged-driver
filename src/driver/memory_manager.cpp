#include "unpd/dispatch.hpp"
#include "unpd/kernel_raii.hpp"

NTSTATUS UnpdHandleAllocate(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_ALLOC_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_ALLOC_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_ALLOC_REQUEST) || outLen < sizeof(UNPD_ALLOC_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->ByteCount == 0 || inBuf->ByteCount > 64 * 1024 * 1024) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T allocSize = static_cast<SIZE_T>(inBuf->ByteCount);
    unpd::NonPagedAllocation<void> memory(allocSize, UNPD_POOL_TAG);

    if (!memory) {
        *information = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    unpd::NonPagedAllocation<UNPD_ALLOCATION_ENTRY> entry(sizeof(UNPD_ALLOCATION_ENTRY), UNPD_POOL_TAG);
    if (!entry) {
        *information = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    constexpr uint32_t kMaxAllocations = 1024;
    constexpr uint64_t kMaxTotalBytes = 256ULL * 1024ULL * 1024ULL;

    uint64_t handle = 0;
    {
        unpd::SpinlockGuard guard(&devExt->StateLock);

        if (devExt->ActiveAllocations >= kMaxAllocations || (devExt->TotalBytesAllocated + allocSize) > kMaxTotalBytes) {
            *information = 0;
            return STATUS_QUOTA_EXCEEDED;
        }

        if (devExt->NextAllocationHandle == 0) {
            devExt->NextAllocationHandle = 1;
        }

        handle = devExt->NextAllocationHandle++;
        entry.get()->Handle = handle;
        entry.get()->Address = memory.release();
        entry.get()->Size = allocSize;
        entry.get()->Tag = UNPD_POOL_TAG;

        InsertTailList(&devExt->AllocationListHead, &entry.get()->ListEntry);
        entry.release();

        devExt->ActiveAllocations++;
        devExt->TotalBytesAllocated += allocSize;
    }

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = UNPD_STATUS_SUCCESS;
    outBuf->AllocatedHandle = handle;
    outBuf->AllocatedSize = allocSize;

    *information = sizeof(UNPD_ALLOC_RESPONSE);
    return STATUS_SUCCESS;
}

NTSTATUS UnpdHandleFree(
    PUNPD_DEVICE_EXTENSION devExt,
    PIRP irp,
    PIO_STACK_LOCATION irpSp,
    ULONG_PTR* information
) {
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    auto* inBuf = static_cast<PUNPD_FREE_REQUEST>(irp->AssociatedIrp.SystemBuffer);
    auto* outBuf = static_cast<PUNPD_FREE_RESPONSE>(irp->AssociatedIrp.SystemBuffer);

    if (inLen < sizeof(UNPD_FREE_REQUEST) || outLen < sizeof(UNPD_FREE_RESPONSE)) {
        *information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inBuf->Magic != UNPD_MAGIC_REQUEST || inBuf->AllocatedHandle == 0) {
        *information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    uint64_t targetHandle = inBuf->AllocatedHandle;
    PVOID freeAddr = nullptr;
    SIZE_T freeSize = 0;
    ULONG freeTag = UNPD_POOL_TAG;
    PUNPD_ALLOCATION_ENTRY targetEntry = nullptr;

    {
        unpd::SpinlockGuard guard(&devExt->StateLock);

        PLIST_ENTRY curr = devExt->AllocationListHead.Flink;
        while (curr != &devExt->AllocationListHead) {
            auto* item = CONTAINING_RECORD(curr, UNPD_ALLOCATION_ENTRY, ListEntry);
            if (item->Handle == targetHandle) {
                RemoveEntryList(&item->ListEntry);
                targetEntry = item;
                freeAddr = item->Address;
                freeSize = item->Size;
                freeTag = item->Tag;

                devExt->ActiveAllocations--;
                devExt->TotalBytesFreed += freeSize;
                break;
            }
            curr = curr->Flink;
        }
    }

    if (targetEntry == nullptr) {
        outBuf->Magic = UNPD_MAGIC_RESPONSE;
        outBuf->Status = UNPD_STATUS_NOT_FOUND;
        outBuf->FreedByteCount = 0;
        *information = sizeof(UNPD_FREE_RESPONSE);
        return STATUS_NOT_FOUND;
    }

    if (freeAddr != nullptr) {
        UnpdFreePool(freeAddr, freeTag);
    }
    UnpdFreePool(targetEntry, UNPD_POOL_TAG);

    outBuf->Magic = UNPD_MAGIC_RESPONSE;
    outBuf->Status = UNPD_STATUS_SUCCESS;
    outBuf->FreedByteCount = static_cast<uint64_t>(freeSize);

    *information = sizeof(UNPD_FREE_RESPONSE);
    return STATUS_SUCCESS;
}
