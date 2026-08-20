#pragma once

#ifndef UNPD_SYSCALL_HPP
#define UNPD_SYSCALL_HPP

#include <windows.h>
#include <winioctl.h>
#include <stdint.h>
#include "unpd/common.h"

namespace unpd::syscall {

/**
 * @brief High-Performance Usermode Direct Kernel Syscall Interface.
 *
 * @details
 * Bypasses high-level Win32 wrappers and executes direct driver gateway requests
 * for CR3 address translation, memory allocations, APC, and stealth operations.
 */
class UnpdSyscallGateway {
public:
    static HANDLE OpenDriverGateway(const wchar_t* devicePath = UNPD_USERMODE_PATH_W) noexcept {
        return CreateFileW(
            devicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
    }

    static bool SyscallPing(HANDLE gateway, uint32_t& outVersionMajor, uint32_t& outVersionMinor) noexcept {
        if (gateway == INVALID_HANDLE_VALUE) return false;

        UNPD_PING_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        UNPD_PING_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_PING,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Magic == UNPD_MAGIC_RESPONSE) {
            outVersionMajor = resp.DriverVersionMajor;
            outVersionMinor = resp.DriverVersionMinor;
            return true;
        }
        return false;
    }

    static bool SyscallReadCr3(HANDLE gateway, uint64_t cr3, uint64_t va, void* userBuf, uint64_t size, uint64_t& outTransferred) noexcept {
        if (gateway == INVALID_HANDLE_VALUE || !userBuf || size == 0) return false;

        UNPD_CR3_MEMORY_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.Cr3 = cr3;
        req.VirtualAddress = va;
        req.UserBuffer = reinterpret_cast<uint64_t>(userBuf);
        req.Size = size;

        UNPD_CR3_MEMORY_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_READ_PROCESS_CR3,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Magic == UNPD_MAGIC_RESPONSE) {
            outTransferred = resp.BytesTransferred;
            return resp.Status == UNPD_STATUS_SUCCESS;
        }
        return false;
    }

    static bool SyscallWriteCr3(HANDLE gateway, uint64_t cr3, uint64_t va, const void* userBuf, uint64_t size, uint64_t& outTransferred) noexcept {
        if (gateway == INVALID_HANDLE_VALUE || !userBuf || size == 0) return false;

        UNPD_CR3_MEMORY_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.Cr3 = cr3;
        req.VirtualAddress = va;
        req.UserBuffer = reinterpret_cast<uint64_t>(const_cast<void*>(userBuf));
        req.Size = size;

        UNPD_CR3_MEMORY_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_WRITE_PROCESS_CR3,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Magic == UNPD_MAGIC_RESPONSE) {
            outTransferred = resp.BytesTransferred;
            return resp.Status == UNPD_STATUS_SUCCESS;
        }
        return false;
    }

    static bool SyscallAllocateNonPaged(HANDLE gateway, uint64_t size, uint64_t& outHandle) noexcept {
        if (gateway == INVALID_HANDLE_VALUE || size == 0) return false;

        UNPD_ALLOC_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.ByteCount = size;

        UNPD_ALLOC_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_ALLOCATE_NONPAGED,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Magic == UNPD_MAGIC_RESPONSE) {
            outHandle = resp.AllocatedHandle;
            return resp.Status == UNPD_STATUS_SUCCESS;
        }
        return false;
    }

    static bool SyscallFreeNonPaged(HANDLE gateway, uint64_t handle) noexcept {
        if (gateway == INVALID_HANDLE_VALUE || handle == 0) return false;

        UNPD_FREE_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.AllocatedHandle = handle;

        UNPD_FREE_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_FREE_NONPAGED,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Magic == UNPD_MAGIC_RESPONSE) {
            return resp.Status == UNPD_STATUS_SUCCESS;
        }
        return false;
    }
};

} // namespace unpd::syscall

#endif // UNPD_SYSCALL_HPP
