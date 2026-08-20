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

    static bool SyscallSimdPatternScan(
        HANDLE gateway,
        uint64_t baseAddress,
        uint64_t bufferSize,
        const uint8_t* pattern,
        uint32_t patternSize,
        const char* mask,
        uint64_t& outMatchAddress
    ) noexcept {
        if (gateway == INVALID_HANDLE_VALUE || baseAddress == 0 || bufferSize == 0 || !pattern || !mask || patternSize > 64) {
            return false;
        }

        UNPD_SIMD_SCAN_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.BaseAddress = baseAddress;
        req.BufferSize = bufferSize;
        req.PatternSize = patternSize;
        memcpy(req.Pattern, pattern, patternSize);
        strncpy_s(req.Mask, mask, _TRUNCATE);

        UNPD_SIMD_SCAN_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_SIMD_PATTERN_SCAN,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Magic == UNPD_MAGIC_RESPONSE) {
            outMatchAddress = resp.MatchAddress;
            return resp.Status == UNPD_STATUS_SUCCESS;
        }
        return false;
    }

    static bool SyscallResolveVmt(
        HANDLE gateway,
        uint64_t moduleBase,
        uint64_t moduleSize,
        uint64_t codeSectionStart,
        uint64_t codeSectionSize,
        uint64_t& outVtableAddr,
        uint32_t& outMethodCount,
        uint64_t& outFirstMethodAddr
    ) noexcept {
        if (gateway == INVALID_HANDLE_VALUE || moduleBase == 0) return false;

        UNPD_RESOLVE_VMT_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.ModuleBase = moduleBase;
        req.ModuleSize = moduleSize;
        req.CodeSectionStart = codeSectionStart;
        req.CodeSectionSize = codeSectionSize;

        UNPD_RESOLVE_VMT_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_RESOLVE_VMT,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Magic == UNPD_MAGIC_RESPONSE) {
            outVtableAddr = resp.VtableAddress;
            outMethodCount = resp.MethodCount;
            outFirstMethodAddr = resp.FirstMethodAddress;
            return resp.Status == UNPD_STATUS_SUCCESS;
        }
        return false;
    }

    static bool SyscallMoveMouseRelative(HANDLE gateway, int32_t deltaX, int32_t deltaY, uint32_t buttonFlags = 0) noexcept {
        if (gateway == INVALID_HANDLE_VALUE) return false;

        UNPD_MOUSE_MOVE_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.DeltaX = deltaX;
        req.DeltaY = deltaY;
        req.ButtonFlags = buttonFlags;

        UNPD_MOUSE_MOVE_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_MOVE_MOUSE_RELATIVE,
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

    static bool SyscallQueryProcessBase(HANDLE gateway, uint32_t processId, uint64_t& outBaseAddr, uint64_t& outPebAddr) noexcept {
        if (gateway == INVALID_HANDLE_VALUE || processId == 0) return false;

        UNPD_PROCESS_BASE_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.ProcessId = processId;

        UNPD_PROCESS_BASE_RESPONSE resp{};
        DWORD bytesReturned = 0;

        bool ok = DeviceIoControl(
            gateway,
            IOCTL_UNPD_QUERY_PROCESS_BASE,
            &req, sizeof(req),
            &resp, sizeof(resp),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Magic == UNPD_MAGIC_RESPONSE) {
            outBaseAddr = resp.BaseAddress;
            outPebAddr = resp.PebAddress;
            return resp.Status == UNPD_STATUS_SUCCESS;
        }
        return false;
    }
};

} // namespace unpd::syscall

#endif // UNPD_SYSCALL_HPP
