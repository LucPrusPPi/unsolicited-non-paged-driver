#include <gtest/gtest.h>
#include <unpd/client.hpp>
#include <vector>

using namespace unpd;

class ClientIntegrationTest : public ::testing::Test {
protected:
    DriverClient client;
};

TEST_F(ClientIntegrationTest, Client_ReadWriteProcessCr3_MockLoopback) {
    const uint64_t dummyCr3 = 0x1AA000ULL;
    const uint64_t dummyVa  = 0x7FFF10000000ULL;
    std::vector<uint8_t> writeBuf = { 0xDE, 0xAD, 0xBE, 0xEF, 0x13, 0x37 };
    size_t written = 0;

    EXPECT_TRUE(client.writeProcessMemoryCr3(dummyCr3, dummyVa, writeBuf.data(), writeBuf.size(), written));
    EXPECT_EQ(written, writeBuf.size());

    std::vector<uint8_t> readBuf(writeBuf.size(), 0);
    size_t bytesRead = 0;
    EXPECT_TRUE(client.readProcessMemoryCr3(dummyCr3, dummyVa, readBuf.data(), readBuf.size(), bytesRead));
    EXPECT_EQ(bytesRead, writeBuf.size());
}

TEST_F(ClientIntegrationTest, Client_QueueUserApc_Success) {
    const uint32_t threadId = 1337;
    void* dummyRoutine = reinterpret_cast<void*>(0x7FFF00400000ULL);
    void* dummyContext = reinterpret_cast<void*>(0xCAFE);

    EXPECT_TRUE(client.queueUserApc(threadId, dummyRoutine, dummyContext));
    EXPECT_FALSE(client.queueUserApc(0, dummyRoutine, dummyContext)); // invalid thread ID
    EXPECT_FALSE(client.queueUserApc(threadId, nullptr, dummyContext)); // invalid routine
}

TEST_F(ClientIntegrationTest, Client_CleanPiDdbCache_ValidParams) {
    EXPECT_TRUE(client.cleanPiDdbCache(L"iqqvw64.sys", 0x5C000000));
    EXPECT_FALSE(client.cleanPiDdbCache(nullptr, 0x5C000000));
    EXPECT_FALSE(client.cleanPiDdbCache(L"", 0x5C000000));
}

TEST_F(ClientIntegrationTest, Client_CleanUnloadedDrivers_ValidParams) {
    EXPECT_TRUE(client.cleanUnloadedDrivers(L"capcom.sys", 0xFFFF888000100000ULL));
    EXPECT_TRUE(client.cleanUnloadedDrivers(L"gdrv.sys", 0));
    EXPECT_TRUE(client.cleanUnloadedDrivers(nullptr, 0xFFFF888000200000ULL));
    EXPECT_FALSE(client.cleanUnloadedDrivers(nullptr, 0)); // both invalid
}

TEST_F(ClientIntegrationTest, SharedRingSession_Lifecycle_PushPopSwap) {
    SharedRingSession session(client, 16);
    ASSERT_TRUE(session.isValid());
    EXPECT_NE(session.getBuffer(), nullptr);
    EXPECT_GE(session.getSize(), 16 * 4096ULL);

    // Send a Ping command into the ring
    EXPECT_TRUE(session.sendCommand(comm::UNPD_OPCODE_PING));

    // Dispatch locally (simulating driver poll)
    EXPECT_EQ(comm::SharedMemoryChannel::PollAndDispatch(session.getBuffer()), STATUS_SUCCESS);

    // Pop the response
    comm::SharedResponse resp{};
    EXPECT_TRUE(session.receiveResponse(resp));
    EXPECT_EQ(resp.Opcode, comm::UNPD_OPCODE_PING);
    EXPECT_EQ(resp.Status, STATUS_SUCCESS);

    // Swap double buffer
    EXPECT_TRUE(session.swapBuffers());
}

TEST_F(ClientIntegrationTest, SharedRingSession_MultiPacketBurst) {
    SharedRingSession session(client, 16);
    ASSERT_TRUE(session.isValid());

    const size_t burstCount = 10;
    for (size_t i = 0; i < burstCount; ++i) {
        uint64_t payloadVal = 0x1000 + i;
        EXPECT_TRUE(session.sendCommand(comm::UNPD_OPCODE_PING, &payloadVal, sizeof(payloadVal)));
    }

    EXPECT_EQ(comm::SharedMemoryChannel::PollAndDispatch(session.getBuffer()), STATUS_SUCCESS);

    for (size_t i = 0; i < burstCount; ++i) {
        comm::SharedResponse resp{};
        EXPECT_TRUE(session.receiveResponse(resp));
        EXPECT_EQ(resp.Opcode, comm::UNPD_OPCODE_PING);
        EXPECT_EQ(resp.Status, STATUS_SUCCESS);
    }
}
