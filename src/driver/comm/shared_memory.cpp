#include <unpd/common.h>
#include <unpd/comm/shared_memory.hpp>
#include <unpd/kernel_asm.hpp>

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <cstring>
#endif

namespace unpd::comm {

NTSTATUS SharedMemoryChannel::Initialize(PVOID sharedPageVa, SIZE_T sizeBytes) noexcept {
    if (!sharedPageVa || sizeBytes < sizeof(SharedChannelHeader)) {
        return STATUS_INVALID_PARAMETER;
    }

    auto* header = static_cast<SharedChannelHeader*>(sharedPageVa);
#ifdef _KERNEL_MODE
    RtlZeroMemory(header, sizeof(SharedChannelHeader));
#else
    std::memset(header, 0, sizeof(SharedChannelHeader));
#endif

    header->Magic = SHARED_MEM_MAGIC_REQ;
    header->Version = SHARED_MEM_VERSION;
    header->ActiveBufferIndex = 0;
    header->Flags = 1; // Channel active
    header->TotalSwaps = 0;
    header->TotalProcessed = 0;
    header->ChannelSize = sizeBytes;
    header->ErrorCount = 0;

    header->Ring.RequestHead = 0;
    header->Ring.RequestTail = 0;
    header->Ring.ResponseHead = 0;
    header->Ring.ResponseTail = 0;

    UnpdMemoryFence();
    return STATUS_SUCCESS;
}

NTSTATUS SharedMemoryChannel::DispatchSingle(
    SharedChannelHeader* channel,
    const SharedCommand& cmd,
    SharedResponse& resp
) noexcept {
    resp.Magic = SHARED_MEM_MAGIC_RESP;
    resp.Opcode = cmd.Opcode;
    resp.Sequence = cmd.Sequence;
    resp.Timestamp = UnpdReadTsc();
    resp.PayloadSize = 0;
    resp.Crc32 = 0;
    resp.Reserved = 0;

    switch (cmd.Opcode) {
        case UNPD_OPCODE_PING: {
            resp.Status = STATUS_SUCCESS;
            resp.PayloadSize = sizeof(uint64_t);
            const uint64_t driverTs = UnpdReadTsc();
            memcpy(resp.Data, &driverTs, sizeof(driverTs));
            break;
        }

        case UNPD_OPCODE_QUERY_STATS: {
            resp.Status = STATUS_SUCCESS;
            resp.PayloadSize = 24;
            auto* stats = reinterpret_cast<uint64_t*>(resp.Data);
            stats[0] = channel->TotalProcessed;
            stats[1] = channel->TotalSwaps;
            stats[2] = channel->ErrorCount;
            break;
        }

        case UNPD_OPCODE_SWAP_BUFFERS: {
            channel->ActiveBufferIndex ^= 1;
            channel->TotalSwaps++;
            UnpdFastSwapBarrier(nullptr, nullptr);
            resp.Status = STATUS_SUCCESS;
            resp.PayloadSize = sizeof(uint32_t);
            const uint32_t activeIdx = channel->ActiveBufferIndex;
            memcpy(resp.Data, &activeIdx, sizeof(activeIdx));
            break;
        }

        default: {
            resp.Status = static_cast<uint32_t>(STATUS_NOT_SUPPORTED);
            channel->ErrorCount++;
            break;
        }
    }

    channel->TotalProcessed++;
    return STATUS_SUCCESS;
}

NTSTATUS SharedMemoryChannel::PollAndDispatch(PVOID sharedPageVa) noexcept {
    if (!sharedPageVa) {
        return STATUS_INVALID_PARAMETER;
    }

    auto* header = static_cast<SharedChannelHeader*>(sharedPageVa);
    if (header->Magic != SHARED_MEM_MAGIC_REQ) {
        return STATUS_INVALID_PARAMETER;
    }

    UnpdLoadFence();
    uint32_t currentTail = header->Ring.RequestTail;
    const uint32_t currentHead = header->Ring.RequestHead;

    while (currentTail != currentHead) {
        const uint32_t slot = currentTail % SHARED_RING_CAPACITY;
        SharedCommand cmd = header->Ring.Commands[slot];

        UnpdLoadFence();
        SharedResponse resp{};
        DispatchSingle(header, cmd, resp);

        const uint32_t respHead = header->Ring.ResponseHead;
        const uint32_t respSlot = respHead % SHARED_RING_CAPACITY;
        header->Ring.Responses[respSlot] = resp;

        UnpdStoreFence();
        header->Ring.ResponseHead = respHead + 1;

        currentTail++;
        header->Ring.RequestTail = currentTail;
        UnpdStoreFence();
    }

    return STATUS_SUCCESS;
}

NTSTATUS SharedMemoryChannel::SwapDoubleBuffer(PVOID sharedPageVa) noexcept {
    if (!sharedPageVa) {
        return STATUS_INVALID_PARAMETER;
    }

    auto* header = static_cast<SharedChannelHeader*>(sharedPageVa);
    header->ActiveBufferIndex ^= 1;
    header->TotalSwaps++;
    UnpdFastSwapBarrier(nullptr, nullptr);
    return STATUS_SUCCESS;
}

bool SharedMemoryChannel::PushCommand(PVOID sharedPageVa, const SharedCommand& cmd) noexcept {
    if (!sharedPageVa) return false;

    auto* header = static_cast<SharedChannelHeader*>(sharedPageVa);
    const uint32_t head = header->Ring.RequestHead;
    const uint32_t tail = header->Ring.RequestTail;

    if (head - tail >= SHARED_RING_CAPACITY) {
        return false; // Ring full
    }

    const uint32_t slot = head % SHARED_RING_CAPACITY;
    header->Ring.Commands[slot] = cmd;
    UnpdStoreFence();
    header->Ring.RequestHead = head + 1;
    UnpdStoreFence();

    return true;
}

bool SharedMemoryChannel::PopResponse(PVOID sharedPageVa, SharedResponse& resp) noexcept {
    if (!sharedPageVa) return false;

    auto* header = static_cast<SharedChannelHeader*>(sharedPageVa);
    const uint32_t head = header->Ring.ResponseHead;
    uint32_t tail = header->Ring.ResponseTail;

    if (tail == head) {
        return false; // Ring empty
    }

    const uint32_t slot = tail % SHARED_RING_CAPACITY;
    UnpdLoadFence();
    resp = header->Ring.Responses[slot];
    header->Ring.ResponseTail = tail + 1;
    UnpdStoreFence();

    return true;
}

} // namespace unpd::comm
