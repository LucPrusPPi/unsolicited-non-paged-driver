#include <gtest/gtest.h>
#include "test_client.hpp"

class PageEngineTest : public ::testing::Test {
protected:
    unpd::test::DriverClient client;
};

TEST_F(PageEngineTest, MapSharedMemory_ValidAddress) {
    uint64_t session = 0;
    void* userAddr = nullptr;
    uint64_t totalBytes = 0;
    uint32_t bufSize = 0;

    EXPECT_TRUE(client.mapSharedMemory(16, session, userAddr, totalBytes, bufSize));
    EXPECT_NE(session, 0ULL);
    EXPECT_NE(userAddr, nullptr);
    EXPECT_EQ(totalBytes, 16u * 4096u);
    EXPECT_EQ(bufSize, 8u * 4096u);

    EXPECT_TRUE(client.unmapSharedMemory(session));
}

TEST_F(PageEngineTest, WriteRead_DirectSharedMemory) {
    uint64_t session = 0;
    void* userAddr = nullptr;
    uint64_t totalBytes = 0;
    uint32_t bufSize = 0;

    ASSERT_TRUE(client.mapSharedMemory(8, session, userAddr, totalBytes, bufSize));
    ASSERT_NE(userAddr, nullptr);

    auto* ptr = static_cast<uint8_t*>(userAddr);
    for (size_t i = 0; i < totalBytes; ++i) {
        ptr[i] = static_cast<uint8_t>((i * 7) & 0xFF);
    }

    for (size_t i = 0; i < totalBytes; ++i) {
        EXPECT_EQ(ptr[i], static_cast<uint8_t>((i * 7) & 0xFF));
    }

    EXPECT_TRUE(client.unmapSharedMemory(session));
}

TEST_F(PageEngineTest, DoubleBufferSwap_Sequence) {
    uint64_t session = 0;
    void* userAddr = nullptr;
    uint64_t totalBytes = 0;
    uint32_t bufSize = 0;

    ASSERT_TRUE(client.mapSharedMemory(16, session, userAddr, totalBytes, bufSize));

    uint32_t active = 0;
    uint32_t standby = 0;
    uint64_t swaps = 0;

    for (int i = 0; i < 50; ++i) {
        ASSERT_TRUE(client.swapBuffers(session, active, standby, swaps));
        EXPECT_NE(active, standby);
        EXPECT_GT(swaps, 0ULL);
    }

    EXPECT_TRUE(client.unmapSharedMemory(session));
}

TEST_F(PageEngineTest, SlabAlloc_Class64B) {
    uint64_t handle = 0;
    uint32_t blockSize = 0;
    EXPECT_TRUE(client.slabAlloc(0, handle, blockSize));
    EXPECT_EQ(blockSize, 64u);
    EXPECT_TRUE(client.slabFree(handle, blockSize));
}

TEST_F(PageEngineTest, SlabAlloc_Class256B) {
    uint64_t handle = 0;
    uint32_t blockSize = 0;
    EXPECT_TRUE(client.slabAlloc(1, handle, blockSize));
    EXPECT_EQ(blockSize, 256u);
    EXPECT_TRUE(client.slabFree(handle, blockSize));
}

TEST_F(PageEngineTest, SlabAlloc_Class1024B) {
    uint64_t handle = 0;
    uint32_t blockSize = 0;
    EXPECT_TRUE(client.slabAlloc(2, handle, blockSize));
    EXPECT_EQ(blockSize, 1024u);
    EXPECT_TRUE(client.slabFree(handle, blockSize));
}

TEST_F(PageEngineTest, SlabAlloc_Class4096B) {
    uint64_t handle = 0;
    uint32_t blockSize = 0;
    EXPECT_TRUE(client.slabAlloc(3, handle, blockSize));
    EXPECT_EQ(blockSize, 4096u);
    EXPECT_TRUE(client.slabFree(handle, blockSize));
}
