#include "unpd/dispatch.hpp"
#include "unpd/kernel_raii.hpp"

#if UNPD_CONFIG_STRICT_SDDL_ACL && defined(_KERNEL_MODE)
extern "C" {
#include <wdmsec.h>
}
#endif

#ifdef _KERNEL_MODE
static PUNPD_DEVICE_EXTENSION g_DevExt = nullptr;

static VOID ProcessNotifyCallbackEx(
    PEPROCESS Process,
    HANDLE ProcessId,
    PPS_CREATE_NOTIFY_INFO CreateInfo
) {
    UNREFERENCED_PARAMETER(Process);
    // If CreateInfo == NULL, the process is exiting
    if (CreateInfo == NULL && g_DevExt != nullptr && g_DevExt->MemoryManager != nullptr) {
        g_DevExt->MemoryManager->HandleProcessExit(ProcessId);
    }
}
#endif

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

#if UNPD_CONFIG_STRICT_SDDL_ACL && defined(_KERNEL_MODE)
    // Strict SDDL ACL: Allow System (SY) and Builtin Admins (BA) Full Access
    UNICODE_STRING sddlString;
    RtlInitUnicodeString(&sddlString, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");

    status = IoCreateDeviceSecure(
        DriverObject,
        sizeof(UNPD_DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &sddlString,
        (LPCGUID)&GUID_DEVCLASS_UNKNOWN,
        &deviceObject
    );
#else
    status = IoCreateDevice(
        DriverObject,
        sizeof(UNPD_DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );
#endif

    if (!NT_SUCCESS(status)) {
        return status;
    }

    auto* devExt = static_cast<PUNPD_DEVICE_EXTENSION>(deviceObject->DeviceExtension);
    RtlZeroMemory(devExt, sizeof(UNPD_DEVICE_EXTENSION));

    devExt->DeviceObject = deviceObject;
    devExt->SymbolicLinkName = symlinkName;
    KeInitializeSpinLock(&devExt->StateLock);
    InitializeListHead(&devExt->AllocationListHead);
    devExt->MemoryManager = static_cast<unpd::memory::UniversalMemoryManager*>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(unpd::memory::UniversalMemoryManager), 'NDPU')
    );
    if (!devExt->MemoryManager) {
        IoDeleteDevice(deviceObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    new (devExt->MemoryManager) unpd::memory::UniversalMemoryManager();
    status = devExt->MemoryManager->Initialize();
    if (!NT_SUCCESS(status)) {
        devExt->MemoryManager->~UniversalMemoryManager();
        ExFreePoolWithTag(devExt->MemoryManager, 'NDPU');
        IoDeleteDevice(deviceObject);
        return status;
    }

#ifdef _KERNEL_MODE
    g_DevExt = devExt;
    PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallbackEx, FALSE);
#endif

    status = IoCreateSymbolicLink(&symlinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
#ifdef _KERNEL_MODE
        PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallbackEx, TRUE);
        g_DevExt = nullptr;
#endif
        devExt->MemoryManager->Shutdown();
        devExt->MemoryManager->~UniversalMemoryManager();
        ExFreePoolWithTag(devExt->MemoryManager, 'NDPU');
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

#ifdef _KERNEL_MODE
        PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallbackEx, TRUE);
        g_DevExt = nullptr;
#endif

        IoDeleteSymbolicLink(&devExt->SymbolicLinkName);

        if (devExt->MemoryManager != nullptr) {
            devExt->MemoryManager->Shutdown();
            devExt->MemoryManager->~UniversalMemoryManager();
            ExFreePoolWithTag(devExt->MemoryManager, 'NDPU');
            devExt->MemoryManager = nullptr;
        }

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
