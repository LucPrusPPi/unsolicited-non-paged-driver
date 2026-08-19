#pragma once

#ifndef UNPD_COMM_SHARED_MEMORY_HPP
#define UNPD_COMM_SHARED_MEMORY_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::comm {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

inline constexpr uint32_t SHARED_MEM_MAGIC_REQ = 0x554E5044; // 'UNPD'
inline constexpr uint32_t SHARED_MEM_MAGIC_RESP= 0x44504E55; // 'DPNU'
inline constexpr uint32_t SHARED_MEM_VERSION   = 0x00020000; // v2.0
inline constexpr size_t   SHARED_RING_CAPACITY = 16;
inline constexpr size_t   SHARED_PAYLOAD_SIZE  = 512;

inline constexpr uint32_t UNPD_OPCODE_PING         = UNPD_IOCTL_INDEX_PING;
inline constexpr uint32_t UNPD_OPCODE_QUERY_STATS  = UNPD_IOCTL_INDEX_QUERY_STATS;
inline constexpr uint32_t UNPD_OPCODE_SWAP_BUFFERS = UNPD_IOCTL_INDEX_SWAP_BUFFERS;
inline constexpr uint32_t UNPD_OPCODE_SLAB_ALLOC   = UNPD_IOCTL_INDEX_SLAB_ALLOC;
inline constexpr uint32_t UNPD_OPCODE_SLAB_FREE    = UNPD_IOCTL_INDEX_SLAB_FREE;

#pragma pack(push, 8)

/**
 * @brief Lockless Command Packet for Shared Memory Ring.
 */
struct SharedCommand {
    volatile uint32_t Magic;
    volatile uint32_t Opcode;
    volatile uint32_t Sequence;
    volatile uint32_t PayloadSize;
    volatile uint64_t Timestamp;
    volatile uint32_t Crc32;
    volatile uint32_t Reserved;
    uint8_t Data[SHARED_PAYLOAD_SIZE];
};

/**
 * @brief Lockless Response Packet for Shared Memory Ring.
 */
struct SharedResponse {
    volatile uint32_t Magic;
    volatile uint32_t Opcode;
    volatile uint32_t Sequence;
    volatile uint32_t Status;
    volatile uint64_t Timestamp;
    volatile uint32_t PayloadSize;
    volatile uint32_t Crc32;
    volatile uint32_t Reserved;
    uint8_t Data[SHARED_PAYLOAD_SIZE];
};

/**
 * @brief Cacheline-aligned lockless ring buffer to eliminate CPU cache bouncing.
 */
struct SharedRingBuffer {
    // Producer / Consumer control fields padded to 64 bytes
    alignas(64) volatile uint32_t RequestHead;
    alignas(64) volatile uint32_t RequestTail;
    alignas(64) volatile uint32_t ResponseHead;
    alignas(64) volatile uint32_t ResponseTail;

    SharedCommand  Commands[SHARED_RING_CAPACITY];
    SharedResponse Responses[SHARED_RING_CAPACITY];
};

/**
 * @brief Master Shared Memory Channel Header (Double Buffer & Control Block).
 */
struct SharedChannelHeader {
    volatile uint32_t Magic;              // 'UNPD'
    volatile uint32_t Version;            // v2.0
    volatile uint32_t ActiveBufferIndex;  // 0 or 1
    volatile uint32_t Flags;              // Channel state flags
    volatile uint64_t TotalSwaps;         // Total double buffer swaps
    volatile uint64_t TotalProcessed;     // Total commands dispatched
    volatile uint64_t ChannelSize;        // Total bytes committed
    volatile uint32_t ErrorCount;         // Malformed/dropped packet count
    volatile uint32_t Reserved;

    SharedRingBuffer Ring;
};

#pragma pack(pop)

using SharedMemoryHeader = SharedChannelHeader;
using SharedMemHeader = SharedChannelHeader;

/**
 * @brief High-throughput, lockless shared-memory communication engine.
 */
class SharedMemoryChannel {
public:
    static NTSTATUS Initialize(PVOID sharedPageVa, SIZE_T sizeBytes = sizeof(SharedChannelHeader)) noexcept;
    static NTSTATUS PollAndDispatch(PVOID sharedPageVa) noexcept;
    static NTSTATUS SwapDoubleBuffer(PVOID sharedPageVa) noexcept;
    static bool     PushCommand(PVOID sharedPageVa, const SharedCommand& cmd) noexcept;
    static bool     PopResponse(PVOID sharedPageVa, SharedResponse& resp) noexcept;

private:
    static NTSTATUS DispatchSingle(SharedChannelHeader* channel, const SharedCommand& cmd, SharedResponse& resp) noexcept;
};

using SharedMemBackend = SharedMemoryChannel;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace unpd::comm

#endif // UNPD_COMM_SHARED_MEMORY_HPP
