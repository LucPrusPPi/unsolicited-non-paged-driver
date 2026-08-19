#include <gtest/gtest.h>
#include <unpd/common.h>
#include <unpd/mmu/physical_memory.hpp>
#include <unpd/mmu/cr3_walker.hpp>
#include <unpd/mmu/pte_remapper.hpp>
#include <unpd/exec/apc.hpp>
#include <unpd/comm/shared_memory.hpp>

TEST(MmuAdvancedTest, PhysicalMemoryValidation) {
    char buf[16] = {};
    SIZE_T rw = 0;
    EXPECT_EQ(unpd::mmu::PhysicalMemory::ReadPhysicalAddress(0, buf, 16, &rw), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::mmu::PhysicalMemory::WritePhysicalAddress(0, buf, 16, &rw), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::mmu::PhysicalMemory::ReadPhysicalAddress(0x1000, buf, 16, &rw), STATUS_SUCCESS);
}

TEST(MmuAdvancedTest, Cr3WalkerValidation) {
    ULONG64 pa = unpd::mmu::Cr3Walker::TranslateVirtualToPhysical(0x2000, 0x7FFF12345678);
    EXPECT_EQ(pa, 0x2000 + 0x678);
}

TEST(ExecAdvancedTest, KernelApcValidation) {
    EXPECT_EQ(unpd::exec::KernelApc::QueueUserApc(nullptr, nullptr, nullptr), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::exec::KernelApc::QueueUserApc((HANDLE)1234, (PVOID)0x7FFF0000, nullptr), STATUS_SUCCESS);
}

TEST(MmuAdvancedTest, PhysicalMemoryMappingRaii) {
    unpd::mmu::PhysicalMemoryMapping<uint32_t> mapping(0x1000);
    EXPECT_TRUE(mapping.IsValid());
    if (mapping.IsValid()) {
        *mapping = 0x1337BEEF;
        EXPECT_EQ(*mapping, 0x1337BEEF);
    }
}

TEST(MmuAdvancedTest, PteRemapperValidation) {
    EXPECT_EQ(unpd::mmu::PteRemapper::MakePageWritable(0, 0), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::mmu::PteRemapper::MakePageWritable(0x1000, 0x7FFF0000), STATUS_SUCCESS);
    EXPECT_EQ(unpd::mmu::PteRemapper::MakePageExecutable(0, 0), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::mmu::PteRemapper::MakePageExecutable(0x1000, 0x7FFF0000), STATUS_SUCCESS);
}

TEST(CommBackendTest, SharedMemValidation) {
    EXPECT_EQ(unpd::comm::SharedMemBackend::Initialize(nullptr), STATUS_INVALID_PARAMETER);
    char sharedBuf[8192] = {};
    EXPECT_EQ(unpd::comm::SharedMemBackend::Initialize(sharedBuf), STATUS_SUCCESS);
    EXPECT_EQ(unpd::comm::SharedMemBackend::PollAndDispatch(), STATUS_SUCCESS);
}

