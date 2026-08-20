#pragma once

#ifndef UNPD_CLIENT_HPP
#define UNPD_CLIENT_HPP

#include <windows.h>
#include <winioctl.h>
#include <string>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstring>
#include "unpd/common.h"
#include "unpd/comm/shared_memory.hpp"

namespace unpd {

/**
 * @brief High-Level C++20 Usermode Client for UNPD Windows Driver.
 *
 * @details
 * Handles connection management to \\.\UnsolicitedNonPagedDriver, zero-copy shared memory
 * double-buffering sessions, slab pool allocations, and automated mock fallback for CI environments.
 */
enum class ClientExecutionMode {
    AutoDetect,
    ForceLiveDriver,
    ForceMockLoopback
};

class DriverClient {
public:
    explicit DriverClient(ClientExecutionMode mode = ClientExecutionMode::AutoDetect)
        : m_handle(INVALID_HANDLE_VALUE), m_mode(mode), m_isMock(false) {
        initMock();
        open(UNPD_USERMODE_PATH_W, mode);
    }

    DriverClient(const std::wstring& devicePath, ClientExecutionMode mode)
        : m_handle(INVALID_HANDLE_VALUE), m_mode(mode), m_isMock(false) {
        initMock();
        open(devicePath, mode);
    }

    ~DriverClient() {
        close();
    }

    DriverClient(const DriverClient&) = delete;
    DriverClient& operator=(const DriverClient&) = delete;

    DriverClient(DriverClient&& other) noexcept 
        : m_handle(other.m_handle), m_mode(other.m_mode), m_isMock(other.m_isMock) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    DriverClient& operator=(DriverClient&& other) noexcept {
        if (this != &other) {
            close();
            m_handle = other.m_handle;
            m_mode = other.m_mode;
            m_isMock = other.m_isMock;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    bool open(const std::wstring& devicePath = UNPD_USERMODE_PATH_W, ClientExecutionMode mode = ClientExecutionMode::AutoDetect) noexcept {
        close();
        m_mode = mode;

        if (m_mode == ClientExecutionMode::ForceMockLoopback) {
            m_isMock = true;
            return true;
        }

        m_handle = CreateFileW(
            devicePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (m_handle != INVALID_HANDLE_VALUE) {
            m_isMock = false;
            return true;
        }

        if (m_mode == ClientExecutionMode::ForceLiveDriver) {
            m_isMock = false;
            return false;
        }

        // AutoDetect fallback for CI test harnesses without kernel driver loaded
        m_isMock = true;
        return true;
    }

    void close() noexcept {
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    [[nodiscard]] bool isOpen() const noexcept {
        return m_handle != INVALID_HANDLE_VALUE || m_isMock;
    }

    [[nodiscard]] bool isMockMode() const noexcept {
        return m_isMock;
    }

    [[nodiscard]] HANDLE handle() const noexcept {
        return m_handle;
    }

    bool ping(uint32_t sequence, UNPD_PING_RESPONSE& response) noexcept {
        if (m_isMock) {
            response.Magic = UNPD_MAGIC_RESPONSE;
            response.Sequence = sequence + 1;
            response.KernelTimestamp = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            );
            response.DriverVersionMajor = 1;
            response.DriverVersionMinor = 0;
            return true;
        }

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
        if (size == 0 || size > 1024ULL * 1024 * 1024) {
            return false;
        }

        if (m_isMock) {
            std::lock_guard<std::mutex> lock(m_mockMutex);
            uint64_t handle = m_mockNextHandle++;
            m_mockAllocations[handle] = size;
            m_mockTotalBytesAllocated += size;
            outHandle = handle;
            return true;
        }

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
        if (handle == 0) return false;

        if (m_isMock) {
            std::lock_guard<std::mutex> lock(m_mockMutex);
            auto it = m_mockAllocations.find(handle);
            if (it != m_mockAllocations.end()) {
                m_mockTotalBytesFreed += it->second;
                m_mockAllocations.erase(it);
                return true;
            }
            return false;
        }

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
        if (m_isMock) {
            std::lock_guard<std::mutex> lock(m_mockMutex);
            stats.Magic = UNPD_MAGIC_RESPONSE;
            stats.ActiveAllocations = static_cast<uint32_t>(m_mockAllocations.size());
            stats.TotalBytesAllocated = m_mockTotalBytesAllocated;
            stats.TotalBytesFreed = m_mockTotalBytesFreed;
            stats.TotalIoctlProcessed = 100;
            stats.SpinLockContentionCount = 0;
            stats.TotalSwapsProcessed = m_mockSwaps;
            stats.ActiveSharedMappings = m_mockSharedSessions.empty() ? 0 : 1;
            return true;
        }

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
        if (pageCount == 0 || pageCount > 256) return false;

        if (m_isMock) {
            std::lock_guard<std::mutex> lock(m_mockMutex);
            uint64_t totalBytes = static_cast<uint64_t>(pageCount) * 4096;
            void* mem = VirtualAlloc(nullptr, static_cast<SIZE_T>(totalBytes), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!mem) return false;
            uint64_t handle = m_mockNextHandle++;
            m_mockSharedSessions[handle] = mem;
            outSessionHandle = handle;
            outUserAddress = mem;
            outTotalBytes = totalBytes;
            outBufferSize = static_cast<uint32_t>(totalBytes / 2);
            return true;
        }

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
        if (sessionHandle == 0) return false;

        if (m_isMock) {
            std::lock_guard<std::mutex> lock(m_mockMutex);
            auto it = m_mockSharedSessions.find(sessionHandle);
            if (it != m_mockSharedSessions.end()) {
                VirtualFree(it->second, 0, MEM_RELEASE);
                m_mockSharedSessions.erase(it);
                return true;
            }
            return false;
        }

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
        if (sessionHandle == 0) return false;

        if (m_isMock) {
            std::lock_guard<std::mutex> lock(m_mockMutex);
            if (m_mockSharedSessions.find(sessionHandle) == m_mockSharedSessions.end()) {
                return false;
            }
            m_mockActiveBufferIndex = (m_mockActiveBufferIndex == 0) ? 1 : 0;
            m_mockSwaps++;
            outActiveIdx = m_mockActiveBufferIndex;
            outStandbyIdx = (m_mockActiveBufferIndex == 0) ? 1 : 0;
            outTotalSwaps = m_mockSwaps;
            return true;
        }

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
        if (blockClass >= 4) return false;

        if (m_isMock) {
            uint32_t sizes[4] = { 64, 256, 1024, 4096 };
            outBlockSize = sizes[blockClass];
            std::lock_guard<std::mutex> lock(m_mockMutex);
            outSlabHandle = m_mockNextHandle++;
            return true;
        }

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
        if (slabHandle == 0 || (blockSize != 64 && blockSize != 256 && blockSize != 1024 && blockSize != 4096)) {
            return false;
        }

        if (m_isMock) {
            return true;
        }

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

    bool readProcessMemoryCr3(uint64_t cr3, uint64_t virtualAddress, void* buffer, size_t size, size_t& bytesRead) noexcept {
        bytesRead = 0;
        if (!cr3 || !virtualAddress || !buffer || size == 0 || size > 16 * 1024 * 1024) {
            return false;
        }

        if (m_isMock) {
            std::memset(buffer, 0xAA, size);
            bytesRead = size;
            return true;
        }

        UNPD_CR3_MEMORY_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.Cr3 = cr3;
        req.VirtualAddress = virtualAddress;
        req.UserBuffer = reinterpret_cast<uint64_t>(buffer);
        req.Size = size;

        UNPD_CR3_MEMORY_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_READ_PROCESS_CR3,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS) {
            bytesRead = static_cast<size_t>(resp.BytesTransferred);
            return true;
        }
        return false;
    }

    bool writeProcessMemoryCr3(uint64_t cr3, uint64_t virtualAddress, const void* buffer, size_t size, size_t& bytesWritten) noexcept {
        bytesWritten = 0;
        if (!cr3 || !virtualAddress || !buffer || size == 0 || size > 16 * 1024 * 1024) {
            return false;
        }

        if (m_isMock) {
            bytesWritten = size;
            return true;
        }

        UNPD_CR3_MEMORY_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.Cr3 = cr3;
        req.VirtualAddress = virtualAddress;
        req.UserBuffer = reinterpret_cast<uint64_t>(const_cast<void*>(buffer));
        req.Size = size;

        UNPD_CR3_MEMORY_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_WRITE_PROCESS_CR3,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        if (ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS) {
            bytesWritten = static_cast<size_t>(resp.BytesTransferred);
            return true;
        }
        return false;
    }

    bool queueUserApc(uint32_t targetThreadId, void* userRoutine, void* userContext) noexcept {
        if (targetThreadId == 0 || userRoutine == nullptr) {
            return false;
        }

        if (m_isMock) {
            return true;
        }

        UNPD_APC_QUEUE_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.TargetThreadId = targetThreadId;
        req.UserRoutine = reinterpret_cast<uint64_t>(userRoutine);
        req.UserContext = reinterpret_cast<uint64_t>(userContext);

        UNPD_APC_QUEUE_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_QUEUE_KAPC,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        return ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS;
    }

    bool cleanPiDdbCache(const wchar_t* driverName, uint32_t timestamp) noexcept {
        if (!driverName || driverName[0] == L'\0') {
            return false;
        }

        if (m_isMock) {
            return true;
        }

        UNPD_STEALTH_PIDDB_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.TimeDateStamp = timestamp;
        wcsncpy_s(req.DriverName, driverName, 63);

        UNPD_STEALTH_PIDDB_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_CLEAN_PIDDB,
            &req,
            static_cast<DWORD>(sizeof(req)),
            &resp,
            static_cast<DWORD>(sizeof(resp)),
            &bytesReturned,
            nullptr
        );

        return ok && bytesReturned == sizeof(resp) && resp.Status == UNPD_STATUS_SUCCESS;
    }

    bool cleanUnloadedDrivers(const wchar_t* driverName, uint64_t bigPoolAddress = 0) noexcept {
        if ((!driverName || driverName[0] == L'\0') && bigPoolAddress == 0) {
            return false;
        }

        if (m_isMock) {
            return true;
        }

        UNPD_STEALTH_UNLOADED_REQUEST req{};
        req.Magic = UNPD_MAGIC_REQUEST;
        req.BigPoolAddress = bigPoolAddress;
        if (driverName) {
            wcsncpy_s(req.DriverName, driverName, 63);
        }

        UNPD_STEALTH_UNLOADED_RESPONSE resp{};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_handle,
            IOCTL_UNPD_CLEAN_UNLOADED,
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
    void initMock() {
        m_mockNextHandle = 100;
        m_mockTotalBytesAllocated = 0;
        m_mockTotalBytesFreed = 0;
        m_mockSwaps = 0;
        m_mockActiveBufferIndex = 0;
    }

    HANDLE m_handle;
    ClientExecutionMode m_mode;
    bool m_isMock;
    std::mutex m_mockMutex;
    uint64_t m_mockNextHandle{ 100 };
    uint64_t m_mockTotalBytesAllocated{ 0 };
    uint64_t m_mockTotalBytesFreed{ 0 };
    uint64_t m_mockSwaps{ 0 };
    uint32_t m_mockActiveBufferIndex{ 0 };
    std::unordered_map<uint64_t, uint64_t> m_mockAllocations;
    std::unordered_map<uint64_t, void*> m_mockSharedSessions;
};

/**
 * @brief High-level RAII helper for managing zero-copy lockless SharedMemoryChannel sessions.
 */
class SharedRingSession {
public:
    SharedRingSession(DriverClient& client, uint32_t pageCount = 16)
        : m_client(client), m_sessionHandle(0), m_userAddress(nullptr), m_totalBytes(0), m_seq(1) {
        uint32_t bufferSize = 0;
        if (!m_client.mapSharedMemory(pageCount, m_sessionHandle, m_userAddress, m_totalBytes, bufferSize)) {
            m_sessionHandle = 0;
            m_userAddress = nullptr;
            m_totalBytes = 0;
        } else if (m_userAddress) {
            comm::SharedMemoryChannel::Initialize(m_userAddress, static_cast<SIZE_T>(m_totalBytes));
        }
    }

    ~SharedRingSession() {
        if (isValid()) {
            m_client.unmapSharedMemory(m_sessionHandle);
            m_sessionHandle = 0;
            m_userAddress = nullptr;
        }
    }

    SharedRingSession(const SharedRingSession&) = delete;
    SharedRingSession& operator=(const SharedRingSession&) = delete;

    SharedRingSession(SharedRingSession&& other) noexcept
        : m_client(other.m_client), m_sessionHandle(other.m_sessionHandle),
          m_userAddress(other.m_userAddress), m_totalBytes(other.m_totalBytes), m_seq(other.m_seq) {
        other.m_sessionHandle = 0;
        other.m_userAddress = nullptr;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return m_sessionHandle != 0 && m_userAddress != nullptr;
    }

    [[nodiscard]] void* getBuffer() const noexcept {
        return m_userAddress;
    }

    [[nodiscard]] size_t getSize() const noexcept {
        return static_cast<size_t>(m_totalBytes);
    }

    bool sendCommand(uint32_t opcode, const void* payload = nullptr, size_t payloadSize = 0) noexcept {
        if (!isValid() || payloadSize > comm::SHARED_PAYLOAD_SIZE) {
            return false;
        }

        comm::SharedCommand cmd{};
        cmd.Magic = comm::SHARED_MEM_MAGIC_REQ;
        cmd.Opcode = opcode;
        cmd.Sequence = m_seq++;
        cmd.PayloadSize = static_cast<uint32_t>(payloadSize);
        cmd.Timestamp = 0;
        if (payload && payloadSize > 0) {
            std::memcpy(const_cast<uint8_t*>(cmd.Data), payload, payloadSize);
        }

        return comm::SharedMemoryChannel::PushCommand(m_userAddress, cmd);
    }

    bool receiveResponse(comm::SharedResponse& resp) noexcept {
        if (!isValid()) {
            return false;
        }
        return comm::SharedMemoryChannel::PopResponse(m_userAddress, resp);
    }

    bool swapBuffers() noexcept {
        if (!isValid()) {
            return false;
        }
        uint32_t active = 0, standby = 0;
        uint64_t swaps = 0;
        return m_client.swapBuffers(m_sessionHandle, active, standby, swaps);
    }

private:
    DriverClient& m_client;
    uint64_t m_sessionHandle;
    void* m_userAddress;
    uint64_t m_totalBytes;
    uint32_t m_seq;
};

namespace test {
    using DriverClient = unpd::DriverClient;
    using SharedRingSession = unpd::SharedRingSession;
}

} // namespace unpd

#endif // UNPD_CLIENT_HPP
