#include <gtest/gtest.h>
#include "unpd/simd/simd_engine.hpp"
#include "unpd/mmu/vmt_resolver.hpp"
#include "unpd/mmu/vad_engine.hpp"
#include "unpd/input/mouse.hpp"
#include "unpd/exec/process_info.hpp"

// Generates 15 intensive test cases for SIMD engine edge cases & stress boundary scans
TEST(SimdStressTest, BoundaryPatternScan64BytesAligned) {
    uint8_t buffer[64]{};
    buffer[60] = 0xAA; buffer[61] = 0xBB; buffer[62] = 0xCC; buffer[63] = 0xDD;
    uint8_t pattern[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    const char mask[] = "xxxx";
    const void* match = unpd::simd::SimdEngine::ScanPattern(buffer, sizeof(buffer), pattern, mask);
    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match, &buffer[60]);
}

TEST(SimdStressTest, BoundaryPatternScanSingleByteMatch) {
    uint8_t buffer[256]{};
    buffer[255] = 0xFF;
    uint8_t pattern[] = { 0xFF };
    const char mask[] = "x";
    const void* match = unpd::simd::SimdEngine::ScanPattern(buffer, sizeof(buffer), pattern, mask);
    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match, &buffer[255]);
}

TEST(SimdStressTest, MultiWildcardPatternScan) {
    uint8_t buffer[128]{};
    buffer[10] = 0x11; buffer[11] = 0x22; buffer[12] = 0x33; buffer[13] = 0x44;
    uint8_t pattern[] = { 0x11, 0x00, 0x33, 0x44 };
    const char mask[] = "x?xx";
    const void* match = unpd::simd::SimdEngine::ScanPattern(buffer, sizeof(buffer), pattern, mask);
    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match, &buffer[10]);
}

TEST(SimdStressTest, FastCopyUnalignedOddSizes) {
    uint8_t src[137];
    uint8_t dest[137];
    for (size_t i = 0; i < sizeof(src); ++i) src[i] = static_cast<uint8_t>(i ^ 0x5A);
    unpd::simd::SimdEngine::FastCopy(dest, src, sizeof(src));
    EXPECT_EQ(memcmp(src, dest, sizeof(src)), 0);
}

TEST(SimdStressTest, FastZeroUnalignedOddSizes) {
    uint8_t dest[193];
    memset(dest, 0xFF, sizeof(dest));
    unpd::simd::SimdEngine::FastZero(dest, sizeof(dest));
    uint8_t zeroes[193]{};
    EXPECT_EQ(memcmp(dest, zeroes, sizeof(dest)), 0);
}

// Additional aggressive stress suite generator
#define GENERATE_AGGR_TEST(id) \
TEST(AggressiveStressTest, StressPatternIteration_##id) { \
    uint8_t buf[512]; \
    memset(buf, 0x00, sizeof(buf)); \
    buf[id * 4] = 0xDE; buf[id * 4 + 1] = 0xAD; \
    uint8_t pat[] = { 0xDE, 0xAD }; \
    const char msk[] = "xx"; \
    const void* match = unpd::simd::SimdEngine::ScanPattern(buf, sizeof(buf), pat, msk); \
    ASSERT_NE(match, nullptr); \
    EXPECT_EQ(match, &buf[id * 4]); \
}

GENERATE_AGGR_TEST(1)
GENERATE_AGGR_TEST(2)
GENERATE_AGGR_TEST(3)
GENERATE_AGGR_TEST(4)
GENERATE_AGGR_TEST(5)
GENERATE_AGGR_TEST(6)
GENERATE_AGGR_TEST(7)
GENERATE_AGGR_TEST(8)
GENERATE_AGGR_TEST(9)
GENERATE_AGGR_TEST(10)
GENERATE_AGGR_TEST(11)
GENERATE_AGGR_TEST(12)
GENERATE_AGGR_TEST(13)
GENERATE_AGGR_TEST(14)
GENERATE_AGGR_TEST(15)
GENERATE_AGGR_TEST(16)
GENERATE_AGGR_TEST(17)
GENERATE_AGGR_TEST(18)
GENERATE_AGGR_TEST(19)
GENERATE_AGGR_TEST(20)

TEST(VadEngineTest, MockVadTreeLookup) {
    unpd::nt::PUNPD_MMVAD_SHORT outNode = nullptr;
    NTSTATUS status = unpd::mmu::VadEngine::FindVadNode(1234, 0x7FF600000000, outNode);
#ifdef _KERNEL_MODE
    EXPECT_EQ(status, STATUS_NOT_FOUND);
#else
    EXPECT_EQ(status, STATUS_NOT_SUPPORTED);
#endif
}

TEST(VadEngineTest, MockVadProtectionMutation) {
    NTSTATUS status = unpd::mmu::VadEngine::ModifyVadProtection(1234, 0x7FF600000000, 0x04);
#ifdef _KERNEL_MODE
    EXPECT_EQ(status, STATUS_NOT_FOUND);
#else
    EXPECT_EQ(status, STATUS_NOT_SUPPORTED);
#endif
}
