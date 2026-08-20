#include <gtest/gtest.h>
#include "unpd/simd/simd_engine.hpp"
#include "unpd/mmu/vmt_resolver.hpp"

TEST(SimdEngineTest, InitializationAndLevelQuery) {
    auto& engine = unpd::simd::SimdEngine::Instance();
    EXPECT_NE(engine.GetActiveLevelName(), nullptr);
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

    auto& engine = unpd::simd::SimdEngine::Instance();
    const void* match = engine.ScanPattern(buffer, sizeof(buffer), pattern, mask);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match, &buffer[64]);
}

TEST(SimdEngineTest, FastZeroAndCopy) {
    uint8_t src[256];
    uint8_t dest[256];
    for (size_t i = 0; i < sizeof(src); ++i) src[i] = static_cast<uint8_t>(i + 1);

    auto& engine = unpd::simd::SimdEngine::Instance();

    engine.FastCopy(dest, src, sizeof(src));
    EXPECT_EQ(memcmp(src, dest, sizeof(src)), 0);

    engine.FastZero(dest, sizeof(dest));
    uint8_t zeroes[256]{};
    EXPECT_EQ(memcmp(dest, zeroes, sizeof(dest)), 0);
}

class TestBase {
public:
    virtual ~TestBase() = default;
    virtual void MethodA() {}
    virtual void MethodB() {}
};

TEST(VmtResolverTest, ZeroRttiVtableResolution) {
    TestBase object;
    void** vtable = *reinterpret_cast<void***>(&object);

    uint64_t vtableAddr = reinterpret_cast<uint64_t>(vtable);
    uint64_t methodAddr = reinterpret_cast<uint64_t>(vtable[0]);

    unpd::mmu::VmtResolver::VmtInfo info{};
    bool found = unpd::mmu::VmtResolver::ResolveVtable(
        reinterpret_cast<void*>(vtableAddr - 16),
        512,
        methodAddr - 0x1000,
        0x10000,
        info
    );

    EXPECT_TRUE(found);
    EXPECT_GE(info.MethodCount, 2u);
}
