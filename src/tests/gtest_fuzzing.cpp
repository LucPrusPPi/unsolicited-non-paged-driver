#include <gtest/gtest.h>
#include "test_client.hpp"

class FuzzingTest : public ::testing::Test {
protected:
    unpd::test::DriverClient client;
};

TEST_F(FuzzingTest, ZeroByteAllocation_Rejected) {
    uint64_t handle = 0;
    EXPECT_FALSE(client.allocateNonPaged(0, handle));
}

TEST_F(FuzzingTest, OversizedAllocation_Rejected) {
    uint64_t handle = 0;
    EXPECT_FALSE(client.allocateNonPaged(1024ULL * 1024 * 1024 * 1024, handle)); // 1 TB
}

TEST_F(FuzzingTest, SlabAlloc_InvalidClass_Rejected) {
    uint64_t handle = 0;
    uint32_t size = 0;
    EXPECT_FALSE(client.slabAlloc(99, handle, size));
}

TEST_F(FuzzingTest, SlabFree_InvalidHandle_Rejected) {
    EXPECT_FALSE(client.slabFree(0, 64));
}

TEST_F(FuzzingTest, MapSharedMemory_ZeroPages_Rejected) {
    uint64_t session = 0;
    void* addr = nullptr;
    uint64_t bytes = 0;
    uint32_t bufSize = 0;
    EXPECT_FALSE(client.mapSharedMemory(0, session, addr, bytes, bufSize));
}

TEST_F(FuzzingTest, MapSharedMemory_OversizedPages_Rejected) {
    uint64_t session = 0;
    void* addr = nullptr;
    uint64_t bytes = 0;
    uint32_t bufSize = 0;
    EXPECT_FALSE(client.mapSharedMemory(10000, session, addr, bytes, bufSize));
}
