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
uint64_t UnpdReadCr2(void);
uint64_t UnpdReadCr3(void);
void     UnpdWriteCr3(uint64_t value);
uint64_t UnpdReadCr4(void);
void     UnpdWriteCr4(uint64_t value);

// TLB & MMU Cache Operations (Ring-0)
void UnpdInvlpg(const void* virtualAddress);
void UnpdWbinvd(void);
void UnpdFlushTlb(void);

// Model-Specific Registers (Ring-0 MSR)
uint64_t UnpdReadMsr(uint32_t msr);
void     UnpdWriteMsr(uint32_t msr, uint64_t value);

// Processor State & Flags
uint64_t UnpdGetRflags(void);
void     UnpdSetRflags(uint64_t flags);

#ifdef __cplusplus
}
#endif

#endif // UNPD_KERNEL_ASM_HPP
