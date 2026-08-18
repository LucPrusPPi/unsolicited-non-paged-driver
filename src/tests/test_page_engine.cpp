#include "test_client.hpp"
#include <vector>
#include <functional>
#include <string>
#include <cstring>

struct TestCase {
    std::string name;
    std::function<bool(unpd::test::DriverClient&)> fn;
};

void RegisterPageEngineTests(std::vector<TestCase>& tests) {
    tests.push_back({
        "PageEngine_MapSharedMemory_ValidAddress",
        [](unpd::test::DriverClient& client) -> bool {
            uint64_t session = 0;
            void* userAddr = nullptr;
            uint64_t totalBytes = 0;
            uint32_t bufSize = 0;

            if (!client.mapSharedMemory(16, session, userAddr, totalBytes, bufSize)) return false;
            if (session == 0 || userAddr == nullptr || totalBytes != 16 * 4096) return false;

            return client.unmapSharedMemory(session);
        }
    });

    tests.push_back({
        "PageEngine_WriteRead_SharedMemoryDirect",
        [](unpd::test::DriverClient& client) -> bool {
            uint64_t session = 0;
            void* userAddr = nullptr;
            uint64_t totalBytes = 0;
            uint32_t bufSize = 0;

            if (!client.mapSharedMemory(8, session, userAddr, totalBytes, bufSize)) return false;

            auto* ptr = static_cast<uint8_t*>(userAddr);
            for (size_t i = 0; i < totalBytes; ++i) {
                ptr[i] = static_cast<uint8_t>(i & 0xFF);
            }

            bool match = true;
            for (size_t i = 0; i < totalBytes; ++i) {
                if (ptr[i] != static_cast<uint8_t>(i & 0xFF)) {
                    match = false;
                    break;
                }
            }

            client.unmapSharedMemory(session);
            return match;
        }
    });

    tests.push_back({
        "PageEngine_AtomicDoubleBufferSwap",
        [](unpd::test::DriverClient& client) -> bool {
            uint64_t session = 0;
            void* userAddr = nullptr;
            uint64_t totalBytes = 0;
            uint32_t bufSize = 0;

            if (!client.mapSharedMemory(16, session, userAddr, totalBytes, bufSize)) return false;

            uint32_t active = 0;
            uint32_t standby = 0;
            uint64_t swaps = 0;

            for (int i = 0; i < 100; ++i) {
                if (!client.swapBuffers(session, active, standby, swaps)) {
                    client.unmapSharedMemory(session);
                    return false;
                }
                if (active == standby) {
                    client.unmapSharedMemory(session);
                    return false;
                }
            }

            return client.unmapSharedMemory(session);
        }
    });

    tests.push_back({
        "PageEngine_SlabAllocAndFree_AllClasses",
        [](unpd::test::DriverClient& client) -> bool {
            for (uint32_t cls = 0; cls < 4; ++cls) {
                uint64_t handle = 0;
                uint32_t blockSize = 0;
                if (!client.slabAlloc(cls, handle, blockSize)) return false;
                if (handle == 0 || blockSize == 0) return false;
                if (!client.slabFree(handle, blockSize)) return false;
            }
            return true;
        }
    });
}
