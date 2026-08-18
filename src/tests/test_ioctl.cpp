#include "test_client.hpp"
#include <vector>
#include <functional>
#include <string>
#include <iostream>

struct TestCase {
    std::string name;
    std::function<bool(unpd::test::DriverClient&)> fn;
};

void RegisterIoctlTests(std::vector<TestCase>& tests) {
    tests.push_back({
        "Ioctl_Ping_ValidSequence",
        [](unpd::test::DriverClient& client) -> bool {
            UNPD_PING_RESPONSE resp{};
            uint32_t seq = 42;
            if (!client.ping(seq, resp)) return false;
            return resp.Sequence == seq + 1 && resp.DriverVersionMajor == 1;
        }
    });

    tests.push_back({
        "Ioctl_AllocateAndFree_SmallBuffer",
        [](unpd::test::DriverClient& client) -> bool {
            uint64_t handle = 0;
            if (!client.allocateNonPaged(1024, handle)) return false;
            if (handle == 0) return false;
            return client.freeNonPaged(handle);
        }
    });

    tests.push_back({
        "Ioctl_AllocateMultiple_UniqueHandles",
        [](unpd::test::DriverClient& client) -> bool {
            std::vector<uint64_t> handles;
            for (int i = 0; i < 16; ++i) {
                uint64_t h = 0;
                if (!client.allocateNonPaged(4096, h)) return false;
                handles.push_back(h);
            }
            for (uint64_t h : handles) {
                if (!client.freeNonPaged(h)) return false;
            }
            return true;
        }
    });

    tests.push_back({
        "Ioctl_FreeInvalidHandle_ReturnsError",
        [](unpd::test::DriverClient& client) -> bool {
            uint64_t bogusHandle = 0xDEADBEEFCAFEBABEULL;
            return !client.freeNonPaged(bogusHandle);
        }
    });

    tests.push_back({
        "Ioctl_QueryStats_ActiveCount",
        [](unpd::test::DriverClient& client) -> bool {
            UNPD_STATS_RESPONSE statsBefore{};
            if (!client.queryStats(statsBefore)) return false;

            uint64_t handle = 0;
            if (!client.allocateNonPaged(2048, handle)) return false;

            UNPD_STATS_RESPONSE statsDuring{};
            if (!client.queryStats(statsDuring)) return false;

            if (statsDuring.ActiveAllocations != statsBefore.ActiveAllocations + 1) return false;

            if (!client.freeNonPaged(handle)) return false;

            UNPD_STATS_RESPONSE statsAfter{};
            if (!client.queryStats(statsAfter)) return false;

            return statsAfter.ActiveAllocations == statsBefore.ActiveAllocations;
        }
    });
}
