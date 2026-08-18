#include "test_client.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <string>

struct TestCase {
    std::string name;
    std::function<bool(unpd::test::DriverClient&)> fn;
};

void RegisterStressTests(std::vector<TestCase>& tests) {
    tests.push_back({
        "Stress_ConcurrentPing_16Threads",
        [](unpd::test::DriverClient& client) -> bool {
            const int threadCount = 16;
            const int iterationsPerThread = 500;
            std::atomic<bool> allPassed{ true };
            std::vector<std::thread> workers;

            for (int t = 0; t < threadCount; ++t) {
                workers.emplace_back([&client, &allPassed, t, iterationsPerThread]() {
                    for (int i = 0; i < iterationsPerThread; ++i) {
                        UNPD_PING_RESPONSE resp{};
                        uint32_t seq = (t * 10000) + i;
                        if (!client.ping(seq, resp)) {
                            allPassed = false;
                            break;
                        }
                        if (resp.Sequence != seq + 1) {
                            allPassed = false;
                            break;
                        }
                    }
                });
            }

            for (auto& th : workers) {
                if (th.joinable()) th.join();
            }

            return allPassed.load();
        }
    });

    tests.push_back({
        "Stress_ConcurrentAllocFree_8Threads",
        [](unpd::test::DriverClient& client) -> bool {
            const int threadCount = 8;
            const int iterationsPerThread = 50;
            std::atomic<bool> allPassed{ true };
            std::vector<std::thread> workers;

            for (int t = 0; t < threadCount; ++t) {
                workers.emplace_back([&client, &allPassed, iterationsPerThread]() {
                    for (int i = 0; i < iterationsPerThread; ++i) {
                        uint64_t handle = 0;
                        if (!client.allocateNonPaged(512 + (i * 64), handle)) {
                            allPassed = false;
                            break;
                        }
                        if (!client.freeNonPaged(handle)) {
                            allPassed = false;
                            break;
                        }
                    }
                });
            }

            for (auto& th : workers) {
                if (th.joinable()) th.join();
            }

            return allPassed.load();
        }
    });
}
