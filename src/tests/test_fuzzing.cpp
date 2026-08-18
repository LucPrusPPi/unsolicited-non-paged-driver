#include "test_client.hpp"
#include <vector>
#include <functional>
#include <string>

struct TestCase {
    std::string name;
    std::function<bool(unpd::test::DriverClient&)> fn;
};

void RegisterFuzzingTests(std::vector<TestCase>& tests) {
    tests.push_back({
        "Fuzzing_Ping_TruncatedInputBuffer",
        [](unpd::test::DriverClient& client) -> bool {
            uint8_t smallBuf[2] = { 0xAA, 0x55 };
            UNPD_PING_RESPONSE resp{};
            DWORD returned = 0;
            BOOL ok = DeviceIoControl(
                client.handle(),
                IOCTL_UNPD_PING,
                smallBuf,
                sizeof(smallBuf),
                &resp,
                sizeof(resp),
                &returned,
                nullptr
            );
            return !ok;
        }
    });

    tests.push_back({
        "Fuzzing_Ping_InvalidMagic",
        [](unpd::test::DriverClient& client) -> bool {
            UNPD_PING_REQUEST req{};
            req.Magic = 0x12345678;
            req.Sequence = 1;
            UNPD_PING_RESPONSE resp{};
            DWORD returned = 0;
            BOOL ok = DeviceIoControl(
                client.handle(),
                IOCTL_UNPD_PING,
                &req,
                sizeof(req),
                &resp,
                sizeof(resp),
                &returned,
                nullptr
            );
            return !ok;
        }
    });

    tests.push_back({
        "Fuzzing_Allocate_ZeroBytes",
        [](unpd::test::DriverClient& client) -> bool {
            uint64_t handle = 0;
            return !client.allocateNonPaged(0, handle);
        }
    });

    tests.push_back({
        "Fuzzing_Allocate_HugeOversizedBytes",
        [](unpd::test::DriverClient& client) -> bool {
            uint64_t handle = 0;
            return !client.allocateNonPaged(1024ULL * 1024 * 1024 * 1024, handle);
        }
    });

    tests.push_back({
        "Fuzzing_ProcessNeither_NullPointers",
        [](unpd::test::DriverClient& client) -> bool {
            DWORD returned = 0;
            BOOL ok = DeviceIoControl(
                client.handle(),
                IOCTL_UNPD_PROCESS_BUFFER_NEITHER,
                nullptr,
                sizeof(UNPD_BUFFER_HEADER),
                nullptr,
                sizeof(UNPD_BUFFER_HEADER),
                &returned,
                nullptr
            );
            return !ok;
        }
    });
}
