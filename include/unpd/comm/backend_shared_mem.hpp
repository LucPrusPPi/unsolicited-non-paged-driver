#pragma once

#ifndef UNPD_COMM_BACKEND_SHARED_MEM_HPP
#define UNPD_COMM_BACKEND_SHARED_MEM_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::comm {

struct SharedMemHeader {
    volatile ULONG Magic;
    volatile ULONG Opcode;
    volatile ULONG Status;
    volatile ULONG PayloadSize;
    UCHAR Payload[4096];
};

class SharedMemBackend {
public:
    static NTSTATUS Initialize(PVOID sharedPageVa);
    static NTSTATUS PollAndDispatch();
};

} // namespace unpd::comm

#endif // UNPD_COMM_BACKEND_SHARED_MEM_HPP
