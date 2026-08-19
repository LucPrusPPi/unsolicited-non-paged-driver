#include <gtest/gtest.h>
#include <unpd/comm/shared_memory.hpp>
#include <vector>
#include <thread>
#include <atomic>

using namespace unpd::comm;

class SharedMemoryChannelTest : public ::testing::Test {
protected:
    std::vector<uint8_t> buffer;
    PVOID channelVa = nullptr;

    void SetUp() override {
        buffer.resize(sizeof(SharedChannelHeader) + 4096, 0);
        channelVa = buffer.data();
        ASSERT_EQ(SharedMemoryChannel::Initialize(channelVa, buffer.size()), STATUS_SUCCESS);
    }
};

TEST_F(SharedMemoryChannelTest, InitializeChannel_LayoutVerification) {
    auto* header = static_cast<SharedChannelHeader*>(channelVa);
    EXPECT_EQ(header->Magic, SHARED_MEM_MAGIC_REQ);
    EXPECT_EQ(header->Version, SHARED_MEM_VERSION);
    EXPECT_EQ(header->ActiveBufferIndex, 0);
    EXPECT_EQ(header->TotalSwaps, 0);
    EXPECT_EQ(header->TotalProcessed, 0);
    EXPECT_EQ(header->Ring.RequestHead, 0);
    EXPECT_EQ(header->Ring.RequestTail, 0);
    EXPECT_EQ(header->Ring.ResponseHead, 0);
    EXPECT_EQ(header->Ring.ResponseTail, 0);
}

TEST_F(SharedMemoryChannelTest, SingleCommand_PushDispatchPop_Ping) {
    SharedCommand cmd{};
    cmd.Magic = SHARED_MEM_MAGIC_REQ;
    cmd.Opcode = UNPD_OPCODE_PING;
    cmd.Sequence = 101;
    cmd.PayloadSize = 0;

    EXPECT_TRUE(SharedMemoryChannel::PushCommand(channelVa, cmd));

    // Dispatch on server/driver side
    EXPECT_EQ(SharedMemoryChannel::PollAndDispatch(channelVa), STATUS_SUCCESS);

    // Pop on client side
    SharedResponse resp{};
    EXPECT_TRUE(SharedMemoryChannel::PopResponse(channelVa, resp));

    EXPECT_EQ(resp.Magic, SHARED_MEM_MAGIC_RESP);
    EXPECT_EQ(resp.Opcode, UNPD_OPCODE_PING);
    EXPECT_EQ(resp.Sequence, 101);
    EXPECT_EQ(resp.Status, STATUS_SUCCESS);
    EXPECT_GT(resp.Timestamp, 0ULL);
}

TEST_F(SharedMemoryChannelTest, RingBuffer_WrapAround_MultiCycle) {
    // Run multiple cycles through ring buffer exceeding capacity (16 slots)
    const size_t totalPackets = 100;

    for (size_t i = 0; i < totalPackets; ++i) {
        SharedCommand cmd{};
        cmd.Magic = SHARED_MEM_MAGIC_REQ;
        cmd.Opcode = UNPD_OPCODE_PING;
        cmd.Sequence = static_cast<uint32_t>(i + 1);

        EXPECT_TRUE(SharedMemoryChannel::PushCommand(channelVa, cmd));
        EXPECT_EQ(SharedMemoryChannel::PollAndDispatch(channelVa), STATUS_SUCCESS);

        SharedResponse resp{};
        EXPECT_TRUE(SharedMemoryChannel::PopResponse(channelVa, resp));
        EXPECT_EQ(resp.Sequence, i + 1);
        EXPECT_EQ(resp.Status, STATUS_SUCCESS);
    }

    auto* header = static_cast<SharedChannelHeader*>(channelVa);
    EXPECT_EQ(header->TotalProcessed, totalPackets);
}

TEST_F(SharedMemoryChannelTest, DoubleBuffer_AtomicSwap_StateVerification) {
    auto* header = static_cast<SharedChannelHeader*>(channelVa);
    EXPECT_EQ(header->ActiveBufferIndex, 0);
    EXPECT_EQ(header->TotalSwaps, 0);

    // Swap 1
    EXPECT_EQ(SharedMemoryChannel::SwapDoubleBuffer(channelVa), STATUS_SUCCESS);
    EXPECT_EQ(header->ActiveBufferIndex, 1);
    EXPECT_EQ(header->TotalSwaps, 1);

    // Swap 2
    EXPECT_EQ(SharedMemoryChannel::SwapDoubleBuffer(channelVa), STATUS_SUCCESS);
    EXPECT_EQ(header->ActiveBufferIndex, 0);
    EXPECT_EQ(header->TotalSwaps, 2);
}

TEST_F(SharedMemoryChannelTest, RingBuffer_FullCapacity_Rejection) {
    // Fill up all 16 slots
    for (size_t i = 0; i < SHARED_RING_CAPACITY; ++i) {
        SharedCommand cmd{};
        cmd.Magic = SHARED_MEM_MAGIC_REQ;
        cmd.Opcode = UNPD_OPCODE_PING;
        cmd.Sequence = static_cast<uint32_t>(i);
        EXPECT_TRUE(SharedMemoryChannel::PushCommand(channelVa, cmd));
    }

    // 17th command must be rejected because ring is full
    SharedCommand overflowCmd{};
    overflowCmd.Magic = SHARED_MEM_MAGIC_REQ;
    overflowCmd.Opcode = UNPD_OPCODE_PING;
    overflowCmd.Sequence = 999;
    EXPECT_FALSE(SharedMemoryChannel::PushCommand(channelVa, overflowCmd));

    // After dispatch and pop, pushing should succeed again
    EXPECT_EQ(SharedMemoryChannel::PollAndDispatch(channelVa), STATUS_SUCCESS);

    SharedResponse dummyResp{};
    EXPECT_TRUE(SharedMemoryChannel::PopResponse(channelVa, dummyResp));

    EXPECT_TRUE(SharedMemoryChannel::PushCommand(channelVa, overflowCmd));
}

TEST_F(SharedMemoryChannelTest, ConcurrentProducerConsumer_Stress) {
    const size_t iterations = 500;
    std::atomic<bool> stopProducer{false};

    std::thread consumer([this, iterations]() {
        size_t processed = 0;
        while (processed < iterations) {
            SharedMemoryChannel::PollAndDispatch(channelVa);

            SharedResponse resp{};
            if (SharedMemoryChannel::PopResponse(channelVa, resp)) {
                EXPECT_EQ(resp.Status, STATUS_SUCCESS);
                processed++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::thread producer([this, iterations]() {
        for (size_t i = 0; i < iterations; ++i) {
            SharedCommand cmd{};
            cmd.Magic = SHARED_MEM_MAGIC_REQ;
            cmd.Opcode = UNPD_OPCODE_PING;
            cmd.Sequence = static_cast<uint32_t>(i);

            while (!SharedMemoryChannel::PushCommand(channelVa, cmd)) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    auto* header = static_cast<SharedChannelHeader*>(channelVa);
    EXPECT_EQ(header->TotalProcessed, iterations);
}
