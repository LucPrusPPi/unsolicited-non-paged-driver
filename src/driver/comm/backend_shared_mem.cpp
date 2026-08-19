#include <unpd/comm/backend_shared_mem.hpp>

namespace unpd::comm {

NTSTATUS SharedMemBackend::Initialize(PVOID sharedPageVa) {
    if (!sharedPageVa) return STATUS_INVALID_PARAMETER;
    return STATUS_SUCCESS;
}

NTSTATUS SharedMemBackend::PollAndDispatch() {
    return STATUS_SUCCESS;
}

} // namespace unpd::comm
