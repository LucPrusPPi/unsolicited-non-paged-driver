#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "test_client.hpp"

class StressTest : public ::testing::Test {
protected:
    unpd::test::DriverClient client;
};

TEST_F(StressTest, ConcurrentPing_16Threads) {
    const int threadCount = 16;
    const int iterationsPerThread = 200;
    std::atomic<bool> allPassed{ true };
    std::vector<std::thread> workers;

    for (int t = 0; t < threadCount; ++t) {
        workers.emplace_back([this, &allPassed, t]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                UNPD_PING_RESPONSE resp{};
                uint32_t seq = (t * 10000) + i;
                if (!client.ping(seq, resp) || resp.Sequence != seq + 1) {
                    allPassed = false;
                    break;
                }
            }
        });
    }

    for (auto& th : workers) {
        if (th.joinable()) th.join();
    }

    EXPECT_TRUE(allPassed.load());
}

TEST_F(StressTest, ConcurrentAllocFree_8Threads) {
    const int threadCount = 8;
    const int iterationsPerThread = 50;
    std::atomic<bool> allPassed{ true };
    std::vector<std::thread> workers;

    for (int t = 0; t < threadCount; ++t) {
        workers.emplace_back([this, &allPassed]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                uint64_t handle = 0;
                if (!client.allocateNonPaged(256 + (i * 16), handle) || !client.freeNonPaged(handle)) {
                    allPassed = false;
                    break;
                }
            }
        });
    }

    for (auto& th : workers) {
        if (th.joinable()) th.join();
    }

    EXPECT_TRUE(allPassed.load());
}

TEST_F(StressTest, ConcurrentBufferSwaps_4Threads) {
    uint64_t session = 0;
    void* userAddr = nullptr;
    uint64_t totalBytes = 0;
    uint32_t bufSize = 0;

    ASSERT_TRUE(client.mapSharedMemory(16, session, userAddr, totalBytes, bufSize));

    const int threadCount = 4;
    const int iterationsPerThread = 100;
    std::atomic<bool> allPassed{ true };
    std::vector<std::thread> workers;

    for (int t = 0; t < threadCount; ++t) {
        workers.emplace_back([this, session, &allPassed]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                uint32_t active = 0;
                uint32_t standby = 0;
                uint64_t swaps = 0;
                if (!client.swapBuffers(session, active, standby, swaps)) {
                    allPassed = false;
                    break;
                }
            }
        });
    }

    for (auto& th : workers) {
        if (th.joinable()) th.join();
    }

    EXPECT_TRUE(allPassed.load());
    EXPECT_TRUE(client.unmapSharedMemory(session));
}
