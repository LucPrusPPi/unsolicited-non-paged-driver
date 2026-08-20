#include <gtest/gtest.h>
#include "unpd/simd/simd_engine.hpp"
#include "unpd/mmu/vmt_resolver.hpp"

TEST(SimdEngineTest, InitializationAndLevelQuery) {
    EXPECT_NE(unpd::simd::SimdEngine::GetActiveLevelName(), nullptr);
}

TEST(SimdEngineTest, PatternScanAVX2OrScalar) {
    uint8_t buffer[128]{};
    for (size_t i = 0; i < sizeof(buffer); ++i) buffer[i] = static_cast<uint8_t>(i);

    // Embed pattern at index 64
    buffer[64] = 0xDE;
    buffer[65] = 0xAD;
    buffer[66] = 0xBE;
    buffer[67] = 0xEF;

    uint8_t pattern[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    const char mask[] = "xxxx";

    const void* match = unpd::simd::SimdEngine::ScanPattern(buffer, sizeof(buffer), pattern, mask);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match, &buffer[64]);
}

TEST(SimdEngineTest, PatternScanAVX512Emulated) {
    uint8_t buffer[256]{};
    for (size_t i = 0; i < sizeof(buffer); ++i) buffer[i] = static_cast<uint8_t>(i);

    // Embed pattern at index 180
    buffer[180] = 0xCA;
    buffer[181] = 0xFE;
    buffer[182] = 0xBA;
    buffer[183] = 0xBE;

    uint8_t pattern[] = { 0xCA, 0xFE, 0xBA, 0xBE };
    const char mask[] = "xxxx";

    const void* match = unpd::simd::SimdEngine::ScanPatternAVX512Emulated(buffer, sizeof(buffer), pattern, mask);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match, &buffer[180]);
}

TEST(SimdEngineTest, FastZeroAndCopy) {
    uint8_t src[256];
    uint8_t dest[256];
    for (size_t i = 0; i < sizeof(src); ++i) src[i] = static_cast<uint8_t>(i + 1);

    unpd::simd::SimdEngine::FastCopy(dest, src, sizeof(src));
    EXPECT_EQ(memcmp(src, dest, sizeof(src)), 0);

    unpd::simd::SimdEngine::FastZero(dest, sizeof(dest));
    uint8_t zeroes[256]{};
    EXPECT_EQ(memcmp(dest, zeroes, sizeof(dest)), 0);
}

static void MockVmtFunction1() {}
static void MockVmtFunction2() {}

TEST(VmtResolverTest, ZeroRttiVtableResolution) {
    uint64_t mockVtable[4] = {
        reinterpret_cast<uint64_t>(&MockVmtFunction1),
        reinterpret_cast<uint64_t>(&MockVmtFunction2),
        0,
        0
    };

    uint64_t codeStart = reinterpret_cast<uint64_t>(&MockVmtFunction1);
    if (reinterpret_cast<uint64_t>(&MockVmtFunction2) < codeStart) {
        codeStart = reinterpret_cast<uint64_t>(&MockVmtFunction2);
    }
    codeStart = (codeStart > 0x1000) ? (codeStart - 0x1000) : 0;

    unpd::mmu::VmtResolver::VmtInfo info{};
    bool found = unpd::mmu::VmtResolver::ResolveVtable(
        mockVtable,
        sizeof(mockVtable),
        codeStart,
        0x100000,
        info
    );

    EXPECT_TRUE(found);
    EXPECT_GE(info.MethodCount, 2u);
}
