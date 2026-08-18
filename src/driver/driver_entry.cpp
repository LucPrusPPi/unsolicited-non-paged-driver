#include "unpd/dispatch.hpp"
#include "unpd/kernel_raii.hpp"

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_OBJECT deviceObject = nullptr;
    UNICODE_STRING deviceName;
    UNICODE_STRING symlinkName;

    RtlInitUnicodeString(&deviceName, UNPD_DEVICE_NAME_W);
    RtlInitUnicodeString(&symlinkName, UNPD_DOS_DEVICE_NAME_W);

    status = IoCreateDevice(
        DriverObject,
        sizeof(UNPD_DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    auto* devExt = static_cast<PUNPD_DEVICE_EXTENSION>(deviceObject->DeviceExtension);
    RtlZeroMemory(devExt, sizeof(UNPD_DEVICE_EXTENSION));

    devExt->DeviceObject = deviceObject;
    devExt->SymbolicLinkName = symlinkName;
    KeInitializeSpinLock(&devExt->StateLock);
    InitializeListHead(&devExt->AllocationListHead);
    devExt->NextAllocationHandle = 1;

    status = IoCreateSymbolicLink(&symlinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = UnpdCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = UnpdCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = UnpdDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

extern "C"
VOID
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
) {
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;

    if (deviceObject != nullptr) {
        auto* devExt = static_cast<PUNPD_DEVICE_EXTENSION>(deviceObject->DeviceExtension);

        IoDeleteSymbolicLink(&devExt->SymbolicLinkName);

        {
            unpd::SpinlockGuard guard(&devExt->StateLock);

            while (!IsListEmpty(&devExt->AllocationListHead)) {
                PLIST_ENTRY entry = RemoveHeadList(&devExt->AllocationListHead);
                auto* alloc = CONTAINING_RECORD(entry, UNPD_ALLOCATION_ENTRY, ListEntry);

                if (alloc->Address != nullptr) {
                    ExFreePoolWithTag(alloc->Address, alloc->Tag);
                }
                ExFreePoolWithTag(alloc, UNPD_POOL_TAG);
            }
        }

        IoDeleteDevice(deviceObject);
    }
}
