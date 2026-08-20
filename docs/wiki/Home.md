# UNPD Framework Documentation

UNPD is a freestanding C++20 Windows NT kernel driver framework featuring a polymorphic memory manager, 4-level CR3 paging walker, lockless shared memory channel, and usermode virtual MMU sandbox.

## Index

- [[Architecture-and-Subsystems]]
- [[MMU-Paging-and-CR3-Walker]]
- [[Polymorphic-Memory-Engine]]
- [[Lockless-Shared-Memory-Ring]]
- [[Zero-BSOD-Safety-Invariants]]
- [[Virtual-MMU-Sandbox]]
- [[IOCTL-Protocol-and-API-Reference]]

## Subsystem Overview

- `unpd::kstd`: Freestanding C++20 kernel primitives (`span`, `expected`, `unique_ptr`, concepts).
- `unpd::mmu`: 4-level PML4 page table walker (`Cr3Walker`), `PhysicalMemoryMapping<T>`, `ProcessAttachmentGuard`.
- `unpd::memory`: Memory strategy hierarchy (`IMemoryEngine`, `MdlMemoryEngine`, `SlabMemoryEngine`, `PoolMemoryEngine`, `DirectNeitherEngine`).
- `unpd::comm`: Lockless ring channel (`SharedMemoryChannel`) with `alignas(64)` cacheline separation and MASM64 barriers (`mfence`, `_mm_lfence`, `_mm_sfence`).
- `unpd::exec`: Asynchronous KAPC dispatcher (`KernelApc`) with `KernelApcRundown` routines.
- `unpd::stealth`: Kernel trace scrubbers (`PiDdbCleaner`, `UnloadedCleaner`).
- `unpd::client`: Usermode C++20 client SDK (`DriverClient`, `SharedRingSession`).
- `unpd::test::emulator`: 64MB `VirtualMmu` sandbox with 64-entry LRU TLB and `#PF` fault generation.
