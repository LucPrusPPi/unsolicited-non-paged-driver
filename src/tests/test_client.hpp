#pragma once

#ifndef UNPD_TEST_CLIENT_HPP
#define UNPD_TEST_CLIENT_HPP

#include <windows.h>
#include <winioctl.h>
#include <string>
#include <stdexcept>
#include <chrono>
#include "unpd/common.h"

namespace unpd::test {

class DriverClient {
public:
    DriverClient() : m_handle(INVALID_HANDLE_VALUE) {}

    explicit DriverClient(const std::wstring& devicePath) : m_handle(INVALID_HANDLE_VALUE) {
        if (!open(devicePath)) {
            throw std::runtime_error("Failed to open driver device handle");
        }
    }

    ~DriverClient() {
        close();
    }

    DriverClient(const DriverClient&) = delete;
    DriverClient& operator=(const DriverClient&) = delete;

    DriverClient(DriverClient&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    DriverClient& operator=(DriverClient&& other) noexcept {
        if (this != &other) {
            close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    bool open(const std::wstring& devicePath = UNPD_USERMODE_PATH_W) noexcept {
        close();
        m_handle = CreateFileW(
            devicePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        return m_handle != INVALID_HANDLE_VALUE;
    }

    void close() noexcept {
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    [[nodiscard]] bool isOpen() const noexcept {
        return m_handle != INVALID_HANDLE_VALUE;
    }

    [[nodiscard]] HANDLE handle() const noexcept {
        return m_handle;
    }

    bool ping(uint32_t sequence, UNPD_PING_RESPONSE& response) noexcept {
        UNPD_PING_REQUEST request{};
        request.Magic = UNPD_MAGIC_REQUEST;
        request.Sequence = sequence;
        request.Timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_PING,
            &request,
            static_cast<DWORD>(sizeof(request)),
            &response,
            static_cast<DWORD>(sizeof(response)),
            &bytesReturned,
            nullptr
        );

        return ok && bytesReturned == sizeof(response) && response.Magic == UNPD_MAGIC_RESPONSE;
    }

    bool allocateNonPaged(uint64_t size, uint64_t& outHandle) noexcept {
        UNPD_ALLOC_REQUEST request{};
        request.Magic = UNPD_MAGIC_REQUEST;
        request.ByteCount = size;
        request.Flags = 0;

        UNPD_ALLOC_RESPONSE response{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_ALLOCATE_NONPAGED,
            &request,
            static_cast<DWORD>(sizeof(request)),
            &response,
            static_cast<DWORD>(sizeof(response)),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(response) && response.Status == UNPD_STATUS_SUCCESS) {
            outHandle = response.AllocatedHandle;
            return true;
        }
        return false;
    }

    bool freeNonPaged(uint64_t handle) noexcept {
        UNPD_FREE_REQUEST request{};
        request.Magic = UNPD_MAGIC_REQUEST;
        request.AllocatedHandle = handle;

        UNPD_FREE_RESPONSE response{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_FREE_NONPAGED,
            &request,
            static_cast<DWORD>(sizeof(request)),
            &response,
            static_cast<DWORD>(sizeof(response)),
            &bytesReturned,
            nullptr
        );

        return ok && bytesReturned == sizeof(response) && response.Status == UNPD_STATUS_SUCCESS;
    }

    bool queryStats(UNPD_STATS_RESPONSE& stats) noexcept {
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_QUERY_STATS,
            nullptr,
            0,
            &stats,
            static_cast<DWORD>(sizeof(stats)),
            &bytesReturned,
            nullptr
        );

        return ok && bytesReturned == sizeof(stats) && stats.Magic == UNPD_MAGIC_RESPONSE;
    }

private:
    HANDLE m_handle;
};

} // namespace unpd::test

#endif // UNPD_TEST_CLIENT_HPP
