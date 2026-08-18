#include <gtest/gtest.h>
#include "test_client.hpp"

class IoctlTest : public ::testing::Test {
protected:
    unpd::test::DriverClient client;
};

TEST_F(IoctlTest, Ping_ValidSequence) {
    UNPD_PING_RESPONSE resp{};
    EXPECT_TRUE(client.ping(100, resp));
    EXPECT_EQ(resp.Magic, UNPD_MAGIC_RESPONSE);
    EXPECT_EQ(resp.Sequence, 101u);
}

TEST_F(IoctlTest, Ping_TimestampPrecision) {
    UNPD_PING_RESPONSE resp{};
    EXPECT_TRUE(client.ping(42, resp));
    EXPECT_GT(resp.KernelTimestamp, 0ULL);
}

TEST_F(IoctlTest, AllocateAndFree_SingleBuffer) {
    uint64_t handle = 0;
    EXPECT_TRUE(client.allocateNonPaged(1024, handle));
    EXPECT_NE(handle, 0ULL);
    EXPECT_TRUE(client.freeNonPaged(handle));
}

TEST_F(IoctlTest, AllocateMultiple_CheckHandles) {
    std::vector<uint64_t> handles;
    for (int i = 0; i < 16; ++i) {
        uint64_t handle = 0;
        ASSERT_TRUE(client.allocateNonPaged(512, handle));
        ASSERT_NE(handle, 0ULL);
        handles.push_back(handle);
    }

    for (uint64_t handle : handles) {
        EXPECT_TRUE(client.freeNonPaged(handle));
    }
}

TEST_F(IoctlTest, Free_InvalidHandle_ReturnsError) {
    EXPECT_FALSE(client.freeNonPaged(0xDEADBEEFCAFEBABE));
}

TEST_F(IoctlTest, QueryStats_MetricsIntegrity) {
    UNPD_STATS_RESPONSE stats{};
    EXPECT_TRUE(client.queryStats(stats));
    EXPECT_EQ(stats.Magic, UNPD_MAGIC_RESPONSE);
}
