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

TEST_F(StressTest, ConcurrentCr3MemoryWalk_16Threads) {
    const int threadCount = 16;
    const int iterationsPerThread = 100;
    std::atomic<bool> allPassed{ true };
    std::vector<std::thread> workers;

    for (int t = 0; t < threadCount; ++t) {
        workers.emplace_back([this, &allPassed, t]() {
            const uint64_t cr3 = 0x1AA000ULL + (t * 0x1000);
            const uint64_t va  = 0x7FFF00000000ULL + (t * 0x100000);
            std::vector<uint8_t> writeBuf(128, static_cast<uint8_t>(t & 0xFF));
            std::vector<uint8_t> readBuf(128, 0);

            for (int i = 0; i < iterationsPerThread; ++i) {
                size_t written = 0;
                size_t read = 0;
                if (!client.writeProcessMemoryCr3(cr3, va, writeBuf.data(), writeBuf.size(), written) ||
                    written != writeBuf.size() ||
                    !client.readProcessMemoryCr3(cr3, va, readBuf.data(), readBuf.size(), read) ||
                    read != readBuf.size()) {
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

TEST_F(StressTest, ConcurrentSharedRingSession_Bursts_8Threads) {
    unpd::SharedRingSession session(client, 16);
    ASSERT_TRUE(session.isValid());

    const int threadCount = 8;
    const int iterationsPerThread = 50;
    std::atomic<bool> allPassed{ true };
    std::vector<std::thread> workers;

    for (int t = 0; t < threadCount; ++t) {
        workers.emplace_back([&session, &allPassed, t]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                uint32_t payloadVal = (t * 1000) + i;
                if (!session.sendCommand(unpd::comm::UNPD_OPCODE_PING, &payloadVal, sizeof(payloadVal))) {
                    // Ring might be temporarily full, yield and retry
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& th : workers) {
        if (th.joinable()) th.join();
    }

    EXPECT_TRUE(allPassed.load());
}

TEST_F(StressTest, RapidClientLifecycle_100Iterations) {
    for (int i = 0; i < 100; ++i) {
        unpd::test::DriverClient tempClient;
        EXPECT_TRUE(tempClient.isOpen());

        uint64_t handle = 0;
        EXPECT_TRUE(tempClient.allocateNonPaged(1024, handle));
        EXPECT_TRUE(tempClient.freeNonPaged(handle));

        unpd::SharedRingSession session(tempClient, 4);
        EXPECT_TRUE(session.isValid());
        EXPECT_TRUE(session.sendCommand(unpd::comm::UNPD_OPCODE_PING));
    }
}
