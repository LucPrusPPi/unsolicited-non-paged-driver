#include <gtest/gtest.h>
#include "unpd/mmu/paging_types.hpp"
#include "unpd/mmu/descriptors.hpp"
#include "unpd/kernel_asm.hpp"
#include "unpd/kstd/kstd_span.hpp"
#include "unpd/kstd/kstd_expected.hpp"
#include "unpd/kstd/kstd_unique_ptr.hpp"

using namespace unpd::mmu;
using namespace unpd::kstd;

class MmuPagingTest : public ::testing::Test {};

TEST_F(MmuPagingTest, StructureSizes_Exact64Bits) {
    EXPECT_EQ(sizeof(VIRTUAL_ADDRESS_64), 8);
    EXPECT_EQ(sizeof(CR3_REGISTER_64), 8);
    EXPECT_EQ(sizeof(PML4_ENTRY_64), 8);
    EXPECT_EQ(sizeof(PDPT_ENTRY_64), 8);
    EXPECT_EQ(sizeof(PD_ENTRY_64), 8);
    EXPECT_EQ(sizeof(PT_ENTRY_64), 8);
}

TEST_F(MmuPagingTest, VirtualAddressDecomposition_CanonicalAddress) {
    // Address: 0x7FFF'ABCD'1234
    VIRTUAL_ADDRESS_64 va{};
    va.Value = 0x7FFFABCD1234ULL;

    EXPECT_EQ(va.Offset4KB, 0x234ULL);
    EXPECT_EQ(va.PtIndex, (0x7FFFABCD1234ULL >> 12) & 0x1FF);
    EXPECT_EQ(va.PdIndex, (0x7FFFABCD1234ULL >> 21) & 0x1FF);
    EXPECT_EQ(va.PdptIndex, (0x7FFFABCD1234ULL >> 30) & 0x1FF);
    EXPECT_EQ(va.Pml4Index, (0x7FFFABCD1234ULL >> 39) & 0x1FF);
}

TEST_F(MmuPagingTest, AlignmentHelpers_ArithmeticCorrectness) {
    EXPECT_EQ(AlignUp(0ULL, 4096), 0ULL);
    EXPECT_EQ(AlignUp(1ULL, 4096), 4096ULL);
    EXPECT_EQ(AlignUp(4095ULL, 4096), 4096ULL);
    EXPECT_EQ(AlignUp(4096ULL, 4096), 4096ULL);
    EXPECT_EQ(AlignUp(4097ULL, 4096), 8192ULL);

    EXPECT_EQ(AlignDown(0ULL, 4096), 0ULL);
    EXPECT_EQ(AlignDown(4095ULL, 4096), 0ULL);
    EXPECT_EQ(AlignDown(4096ULL, 4096), 4096ULL);
    EXPECT_EQ(AlignDown(8191ULL, 4096), 4096ULL);

    EXPECT_TRUE(IsAligned(0ULL, 4096));
    EXPECT_TRUE(IsAligned(4096ULL, 4096));
    EXPECT_FALSE(IsAligned(4097ULL, 4096));
}

TEST_F(MmuPagingTest, LargePageOffsets_2MBAnd1GB) {
    VIRTUAL_ADDRESS_64 va{};
    va.Value = 0xFFFF800012345678ULL;

    EXPECT_EQ(va.Offset2MB, 0x145678ULL);
    EXPECT_EQ(va.Offset1GB, 0x12345678ULL);
}

TEST_F(MmuPagingTest, PtEntry_BitfieldsVerification) {
    PT_ENTRY_64 pte{};
    pte.Present = 1;
    pte.ReadWrite = 1;
    pte.UserSupervisor = 0;
    pte.PageFrameNumber = 0x123456;
    pte.ExecuteDisable = 1;

    EXPECT_EQ(pte.Present, 1);
    EXPECT_EQ(pte.ReadWrite, 1);
    EXPECT_EQ(pte.PageFrameNumber, 0x123456ULL);
    EXPECT_EQ(pte.ExecuteDisable, 1);
}

TEST_F(MmuPagingTest, KstdSpan_MemoryViewOperations) {
    uint8_t buffer[64] = { 0 };
    for (int i = 0; i < 64; ++i) buffer[i] = static_cast<uint8_t>(i);

    byte_span s(buffer, 64);
    EXPECT_EQ(s.size(), 64);
    EXPECT_EQ(s.size_bytes(), 64);
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(s[10], 10);

    auto sub = s.subspan(10, 20);
    EXPECT_EQ(sub.size(), 20);
    EXPECT_EQ(sub[0], 10);
    EXPECT_EQ(sub[19], 29);
}

TEST_F(MmuPagingTest, KstdExpected_SuccessAndErrorHandling) {
    expected<int, int> okVal(42);
    EXPECT_TRUE(okVal.has_value());
    EXPECT_EQ(okVal.value(), 42);

    expected<int, int> errVal = expected<int, int>::error(-1);
    EXPECT_FALSE(errVal.has_value());
    EXPECT_EQ(errVal.error(), -1);
    EXPECT_EQ(errVal.value_or(100), 100);
}

TEST_F(MmuPagingTest, DescriptorStructures_ExactSizes) {
    EXPECT_EQ(sizeof(DESCRIPTOR_TABLE_REGISTER_64), 10);
    EXPECT_EQ(sizeof(IDT_ENTRY_64), 16);
    EXPECT_EQ(sizeof(GDT_ENTRY_64), 8);
    EXPECT_EQ(sizeof(TSS64), 104);
    EXPECT_EQ(sizeof(DR7_REGISTER_64), 8);
    EXPECT_EQ(sizeof(CR0_REGISTER_64), 8);
    EXPECT_EQ(sizeof(CR4_REGISTER_64), 8);
    EXPECT_EQ(sizeof(IA32_EFER_REGISTER_64), 8);
    EXPECT_EQ(sizeof(IA32_PAT_REGISTER_64), 8);
}

TEST_F(MmuPagingTest, IdtEntry_OffsetPackingAndUnpacking) {
    IDT_ENTRY_64 idt{};
    const uint64_t handler = 0xFFFFF80012345678ULL;

    idt.SetOffset(handler);
    idt.Selector = 0x10;
    idt.Type = 0xE; // 64-bit Interrupt Gate
    idt.Present = 1;
    idt.Dpl = 0;

    EXPECT_EQ(idt.GetOffset(), handler);
    EXPECT_EQ(idt.Selector, 0x10);
    EXPECT_EQ(idt.Type, 0xE);
    EXPECT_EQ(idt.Present, 1);
    EXPECT_EQ(idt.Dpl, 0);
}

TEST_F(MmuPagingTest, Dr7Register_BitfieldDecomposition) {
    DR7_REGISTER_64 dr7{};
    dr7.L0 = 1;         // Enable DR0 local breakpoint
    dr7.RW0 = 0b01;     // Break on data write
    dr7.LEN0 = 0b10;    // 8-byte watchpoint length
    dr7.GE = 1;         // Global exact

    EXPECT_EQ(dr7.L0, 1);
    EXPECT_EQ(dr7.RW0, 1);
    EXPECT_EQ(dr7.LEN0, 2);
    EXPECT_EQ(dr7.GE, 1);
    EXPECT_NE(dr7.Value, 0ULL);
}

TEST_F(MmuPagingTest, HardwareCrc32_ComputationCorrectness) {
    const char testStr[] = "UNPD_HARDWARE_CRC32_BENCHMARK";
    uint32_t crc = UnpdComputeCrc32_Buffer(0, testStr, sizeof(testStr) - 1);
    EXPECT_NE(crc, 0U);

    // Byte by byte incremental consistency
    uint32_t stepCrc = 0;
    for (size_t i = 0; i < sizeof(testStr) - 1; ++i) {
        stepCrc = UnpdComputeCrc32_u8(stepCrc, static_cast<uint8_t>(testStr[i]));
    }
    EXPECT_EQ(crc, stepCrc);
}

TEST_F(MmuPagingTest, AtomicBitwisePrimitives_Operations) {
    int64_t bitmap = 0;

    // Test BitSet
    uint32_t oldBit0 = UnpdAtomicBitSet(&bitmap, 0);
    EXPECT_EQ(oldBit0, 0U);
    EXPECT_EQ(bitmap, 1LL);

    uint32_t oldBit42 = UnpdAtomicBitSet(&bitmap, 42);
    EXPECT_EQ(oldBit42, 0U);
    EXPECT_TRUE(UnpdAtomicBitTest(&bitmap, 42));

    // Test BitReset
    uint32_t oldBitReset = UnpdAtomicBitReset(&bitmap, 0);
    EXPECT_EQ(oldBitReset, 1U);
    EXPECT_FALSE(UnpdAtomicBitTest(&bitmap, 0));
    EXPECT_TRUE(UnpdAtomicBitTest(&bitmap, 42));
}

TEST_F(MmuPagingTest, HardwarePrimitives_ExtendedMasm) {
    char testBuf[64] = "MASM_HARDWARE_PRIMITIVE_TEST";
    UnpdPause();
    UnpdClflush(testBuf);
    UnpdClwb(testBuf);
    EXPECT_EQ(testBuf[0], 'M');
}

