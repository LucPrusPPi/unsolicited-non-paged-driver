#pragma once

#ifndef UNPD_COMM_SHARED_MEMORY_HPP
#define UNPD_COMM_SHARED_MEMORY_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::comm {

struct SharedMemoryHeader {
    volatile ULONG Magic;
    volatile ULONG Opcode;
    volatile ULONG Status;
    volatile ULONG PayloadSize;
    UCHAR Payload[4096];
};

using SharedMemHeader = SharedMemoryHeader;

class SharedMemoryChannel {
public:
    static NTSTATUS Initialize(PVOID sharedPageVa);
    static NTSTATUS PollAndDispatch();
};

using SharedMemBackend = SharedMemoryChannel;

} // namespace unpd::comm

#endif // UNPD_COMM_SHARED_MEMORY_HPP
