# IOCTL Protocol & API Reference

## Protocol Overview

The UNPD driver exposes **16 IOCTL opcodes** (`0x800` through `0x80F`) using `FILE_DEVICE_UNKNOWN` (Device Type `0x22`).

---

## Complete IOCTL Registry

| Opcode | IOCTL Code | Function Name | Description | Method |
|---|---|---|---|---|
| `0x800` | `0x222000` | `UNPD_IOCTL_PING` | Connectivity probe & version query | `METHOD_BUFFERED` |
| `0x801` | `0x222004` | `UNPD_IOCTL_ALLOC_NONPAGED` | Allocate tracked NonPagedPoolNx block | `METHOD_BUFFERED` |
| `0x802` | `0x222008` | `UNPD_IOCTL_FREE_NONPAGED` | Free tracked NonPagedPoolNx block | `METHOD_BUFFERED` |
| `0x803` | `0x22200C` | `UNPD_IOCTL_READ_PROCESS_CR3` | Read process memory via PML4 walk | `METHOD_BUFFERED` |
| `0x804` | `0x222010` | `UNPD_IOCTL_WRITE_PROCESS_CR3` | Write process memory via PML4 walk | `METHOD_BUFFERED` |
| `0x805` | `0x222014` | `UNPD_IOCTL_MAP_SHARED_MEM` | Create zero-copy MDL shared mapping | `METHOD_BUFFERED` |
| `0x806` | `0x222018` | `UNPD_IOCTL_UNMAP_SHARED_MEM` | Release MDL shared mapping | `METHOD_BUFFERED` |
| `0x807` | `0x22201C` | `UNPD_IOCTL_QUEUE_USER_APC` | Queue asynchronous User APC to thread | `METHOD_BUFFERED` |
| `0x808` | `0x222020` | `UNPD_IOCTL_CLEAN_PIDDB` | Scrub PiDDBCacheTable AVL node | `METHOD_BUFFERED` |
| `0x809` | `0x222024` | `UNPD_IOCTL_CLEAN_UNLOADED` | Compact MmUnloadedDrivers circular list | `METHOD_BUFFERED` |
| `0x80A` | `0x222028` | `UNPD_IOCTL_SLAB_ALLOCATE` | Allocate O(1) Lookaside Slab block | `METHOD_BUFFERED` |
| `0x80B` | `0x22202C` | `UNPD_IOCTL_SLAB_FREE` | Free O(1) Lookaside Slab block | `METHOD_BUFFERED` |
| `0x80C` | `0x222030` | `UNPD_IOCTL_INIT_SHARED_RING` | Initialize lockless ring buffer channel | `METHOD_BUFFERED` |
| `0x80D` | `0x222034` | `UNPD_IOCTL_SWAP_RING_BUFFER` | Atomic double-buffer swap (`mfence`) | `METHOD_BUFFERED` |
| `0x80E` | `0x222038` | `UNPD_IOCTL_POLL_RING_DISPATCH` | Flush & dispatch pending ring packets | `METHOD_BUFFERED` |
| `0x80F` | `0x22203C` | `UNPD_IOCTL_GET_TELEMETRY` | Query driver allocation & I/O statistics | `METHOD_BUFFERED` |

---

## Usermode C++20 Client SDK (`DriverClient`)

The client SDK (`include/unpd/client.hpp`) wraps all 16 IOCTL services into strongly typed, exception-free methods:

```cpp
#include <unpd/client.hpp>

unpd::client::DriverClient client;
if (client.Connect()) {
    // 1. Ping probe
    uint32_t version = 0;
    if (client.Ping(version)) {
        printf("Connected to UNPD Driver v%u\n", version);
    }

    // 2. Read memory via CR3 walk
    uint64_t targetCr3 = 0x1AA000;
    uint64_t targetVa = 0x7FF600000000;
    char buffer[256]{};
    SIZE_T bytesRead = 0;
    if (client.ReadProcessMemoryCr3(targetCr3, targetVa, buffer, sizeof(buffer), &bytesRead)) {
        printf("Read %zu bytes from target process\n", bytesRead);
    }

    // 3. Lockless Shared Memory Ring Session
    auto ringSession = client.CreateSharedRingSession(16);
    if (ringSession.IsValid()) {
        ringSession.PushCommand(UNPD_OPCODE_PING, nullptr, 0);
        ringSession.SwapBuffer();
    }
}
```
