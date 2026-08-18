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

    bool mapSharedMemory(
        uint32_t pageCount,
        uint64_t& outSessionHandle,
        void*& outUserAddress,
        uint64_t& outTotalBytes,
        uint32_t& outBufferSize
    ) noexcept {
        UNPD_MAP_SHARED_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.PageCount = pageCount;

        UNPD_MAP_SHARED_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_MAP_SHARED_MEMORY,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS) {
            outSessionHandle = resp.SessionHandle;
            outUserAddress = reinterpret_cast<void*>(resp.UserAddress);
            outTotalBytes = resp.TotalBytes;
            outBufferSize = resp.BufferSize;
            return true;
        }
        return false;
    }

    bool unmapSharedMemory(uint64_t sessionHandle) noexcept {
        UNPD_UNMAP_SHARED_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.SessionHandle = sessionHandle;

        UNPD_UNMAP_SHARED_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_UNMAP_SHARED_MEMORY,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        return ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS;
    }

    bool swapBuffers(
        uint64_t sessionHandle,
        uint32_t& outActiveIdx,
        uint32_t& outStandbyIdx,
        uint64_t& outTotalSwaps
    ) noexcept {
        UNPD_SWAP_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.SessionHandle = sessionHandle;

        UNPD_SWAP_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_SWAP_BUFFERS,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS) {
            outActiveIdx = resp.ActiveBufferIndex;
            outStandbyIdx = resp.StandbyBufferIndex;
            outTotalSwaps = resp.TotalSwaps;
            return true;
        }
        return false;
    }

    bool slabAlloc(uint32_t blockClass, uint64_t& outSlabHandle, uint32_t& outBlockSize) noexcept {
        UNPD_SLAB_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.BlockClass = blockClass;

        UNPD_SLAB_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_SLAB_ALLOC,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS) {
            outSlabHandle = resp.SlabHandle;
            outBlockSize = resp.BlockSize;
            return true;
        }
        return false;
    }

    bool slabFree(uint64_t slabHandle, uint32_t blockSize) noexcept {
        UNPD_SLAB_RESPONSE req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.SlabHandle = slabHandle;
        req.BlockSize = blockSize;

        UNPD_FREE_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_SLAB_FREE,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        return ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS;
    }

private:
    HANDLE m_handle;
};

} // namespace unpd::test

#endif // UNPD_TEST_CLIENT_HPP
