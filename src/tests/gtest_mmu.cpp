#include <gtest/gtest.h>
#include "unpd/mmu/paging_types.hpp"
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
