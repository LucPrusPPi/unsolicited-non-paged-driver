#include <unpd/common.h>
#include <unpd/comm/shared_memory.hpp>
#include <unpd/kernel_asm.hpp>

namespace unpd::comm {

static SharedMemHeader* g_sharedPage = nullptr;

NTSTATUS SharedMemBackend::Initialize(PVOID sharedPageVa) {
    if (!sharedPageVa) {
        return STATUS_INVALID_PARAMETER;
    }
    g_sharedPage = static_cast<SharedMemHeader*>(sharedPageVa);
    g_sharedPage->Magic = 0x504D5553; // 'SUMP'
    g_sharedPage->Status = 0;
    g_sharedPage->Opcode = 0;
    g_sharedPage->PayloadSize = 0;
    UnpdMemoryFence();
    return STATUS_SUCCESS;
}

NTSTATUS SharedMemBackend::PollAndDispatch() {
    if (!g_sharedPage) {
        return STATUS_INVALID_PARAMETER;
    }

    UnpdMemoryFence();
    if (g_sharedPage->Opcode != 0 && g_sharedPage->Status == 0) {
        switch (g_sharedPage->Opcode) {
            case 1: // PING
                g_sharedPage->Status = static_cast<ULONG>(STATUS_SUCCESS);
                break;
            default:
                g_sharedPage->Status = static_cast<ULONG>(STATUS_NOT_SUPPORTED);
                break;
        }
        UnpdMemoryFence();
    }
    return STATUS_SUCCESS;
}

} // namespace unpd::comm

