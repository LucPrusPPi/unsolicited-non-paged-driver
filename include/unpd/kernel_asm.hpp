#pragma once

#ifndef UNPD_KERNEL_ASM_HPP
#define UNPD_KERNEL_ASM_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hardware Timestamp & Performance
uint64_t UnpdReadTsc(void);
uint64_t UnpdReadTscp(uint32_t* aux);

// Hardware Memory Barriers & Streaming Fences
void UnpdMemoryFence(void);
void UnpdLoadFence(void);
void UnpdStoreFence(void);
void UnpdFastSwapBarrier(void* activePtr, void* standbyPtr);

// Fast Block Streaming Operations (x64 REP strings)
void UnpdFastCopy64(void* destination, const void* source, uint64_t qwordCount);
void UnpdFastZero64(void* destination, uint64_t qwordCount);

// CPU Topology & Hardware Capabilities
void UnpdCpuId(int32_t cpuInfo[4], int32_t functionId);

// Architectural Control Registers (Ring-0)
uint64_t UnpdReadCr0(void);
void     UnpdWriteCr0(uint64_t value);
void     UnpdDisableWriteProtect(void);
void     UnpdEnableWriteProtect(void);
uint64_t UnpdReadCr2(void);
uint64_t UnpdReadCr3(void);
void     UnpdWriteCr3(uint64_t value);
uint64_t UnpdReadCr4(void);
void     UnpdWriteCr4(uint64_t value);
uint64_t UnpdReadCr8(void);
void     UnpdWriteCr8(uint64_t value);

// TLB & MMU Cache Operations (Ring-0)
void UnpdInvlpg(const void* virtualAddress);
void UnpdWbinvd(void);
void UnpdFlushTlb(void);
void UnpdClflush(const void* address);
void UnpdClflushopt(const void* address);
void UnpdClwb(const void* address);

// CPU Execution & Interrupt Synchronization
void UnpdPause(void);
void UnpdCli(void);
void UnpdSti(void);
void UnpdSwapGs(void);
void UnpdVmxoff(void);

// Hardware I/O Port Access
uint8_t  UnpdInByte(uint16_t port);
void     UnpdOutByte(uint16_t port, uint8_t value);
uint16_t UnpdInWord(uint16_t port);
void     UnpdOutWord(uint16_t port, uint16_t value);
uint32_t UnpdInDword(uint16_t port);
void     UnpdOutDword(uint16_t port, uint32_t value);

// Model-Specific Registers (Ring-0 MSR)
uint64_t UnpdReadMsr(uint32_t msr);
void     UnpdWriteMsr(uint32_t msr, uint64_t value);

// Processor State & Flags
uint64_t UnpdGetRflags(void);
void     UnpdSetRflags(uint64_t flags);

// Descriptor Table Registers
void     UnpdGetGdt(void* gdtr);
void     UnpdGetIdt(void* idtr);
uint16_t UnpdGetTr(void);
uint16_t UnpdGetLdtr(void);

// Segment Selectors
uint16_t UnpdGetCs(void);
uint16_t UnpdGetDs(void);
uint16_t UnpdGetEs(void);
uint16_t UnpdGetSs(void);
uint16_t UnpdGetFs(void);
uint16_t UnpdGetGs(void);

// Hardware Debug Registers (DR0..DR7)
uint64_t UnpdReadDr0(void);
void     UnpdWriteDr0(uint64_t value);
uint64_t UnpdReadDr1(void);
void     UnpdWriteDr1(uint64_t value);
uint64_t UnpdReadDr2(void);
void     UnpdWriteDr2(uint64_t value);
uint64_t UnpdReadDr3(void);
void     UnpdWriteDr3(uint64_t value);
uint64_t UnpdReadDr6(void);
void     UnpdWriteDr6(uint64_t value);
uint64_t UnpdReadDr7(void);
void     UnpdWriteDr7(uint64_t value);

// Extended Control Registers & Performance Counters
uint64_t UnpdGetXcr0(void);
void     UnpdSetXcr0(uint64_t value);
uint64_t UnpdReadPmc(uint32_t counter);

// Atomic Bitwise Operations
uint32_t UnpdAtomicBitSet(volatile int64_t* base, int64_t bit);
uint32_t UnpdAtomicBitReset(volatile int64_t* base, int64_t bit);
uint32_t UnpdAtomicBitTest(const volatile int64_t* base, int64_t bit);

// Hardware SSE4.2 CRC32 Primitives
uint32_t UnpdComputeCrc32_u8(uint32_t initialCrc, uint8_t data);
uint32_t UnpdComputeCrc32_u32(uint32_t initialCrc, uint32_t data);
uint64_t UnpdComputeCrc32_u64(uint64_t initialCrc, uint64_t data);
uint32_t UnpdComputeCrc32_Buffer(uint32_t initialCrc, const void* buffer, uint64_t length);

#ifdef __cplusplus
}
#endif

#endif // UNPD_KERNEL_ASM_HPP
