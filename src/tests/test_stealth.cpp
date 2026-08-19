#include <gtest/gtest.h>
#include <unpd/common.h>
#include <unpd/stealth/piddb.hpp>
#include <unpd/stealth/unloaded_drivers.hpp>
#include <vector>
#include <cstring>

namespace {

const uint8_t* ScanPattern(const uint8_t* base, size_t size, const uint8_t* pattern, const char* mask) {
    if (!base || !pattern || !mask || size == 0) return nullptr;
    const size_t maskLen = std::strlen(mask);
    if (maskLen == 0 || size < maskLen) return nullptr;

    for (size_t i = 0; i <= size - maskLen; ++i) {
        bool found = true;
        for (size_t j = 0; j < maskLen; ++j) {
            if (mask[j] != '?' && base[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return nullptr;
}

} // anonymous namespace

TEST(StealthTest, StructureLayouts_Verification) {
    EXPECT_EQ(sizeof(unpd::stealth::PiDdbCacheEntry), sizeof(LIST_ENTRY) + sizeof(UNICODE_STRING) + sizeof(ULONG) + sizeof(NTSTATUS));
    EXPECT_EQ(sizeof(unpd::stealth::UnloadedDriverEntry), sizeof(UNICODE_STRING) + sizeof(PVOID) + sizeof(PVOID) + sizeof(LARGE_INTEGER));
}

TEST(StealthTest, PatternScanner_ExactMatch) {
    const std::vector<uint8_t> memory = { 0x90, 0x90, 0x48, 0x8B, 0x05, 0x12, 0x34, 0x56, 0x78, 0xC3 };
    const uint8_t pattern[] = { 0x48, 0x8B, 0x05 };
    const char mask[] = "xxx";

    const uint8_t* result = ScanPattern(memory.data(), memory.size(), pattern, mask);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result - memory.data(), 2);
}

TEST(StealthTest, PatternScanner_WildcardMatch) {
    const std::vector<uint8_t> memory = { 0x48, 0x8D, 0x0D, 0xAA, 0xBB, 0xCC, 0xDD, 0xE8 };
    const uint8_t pattern[] = { 0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE8 };
    const char mask[] = "xxx????x";

    const uint8_t* result = ScanPattern(memory.data(), memory.size(), pattern, mask);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result - memory.data(), 0);
}

TEST(StealthTest, PatternScanner_NotFound_ReturnsNull) {
    const std::vector<uint8_t> memory = { 0x90, 0x90, 0xCC, 0xC3 };
    const uint8_t pattern[] = { 0x48, 0x89, 0x5C };
    const char mask[] = "xxx";

    const uint8_t* result = ScanPattern(memory.data(), memory.size(), pattern, mask);
    EXPECT_EQ(result, nullptr);
}
