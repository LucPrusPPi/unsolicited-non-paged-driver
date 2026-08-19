#include <gtest/gtest.h>
#include <unpd/common.h>
#include <unpd/stealth/piddb.hpp>
#include <unpd/stealth/unloaded_drivers.hpp>

TEST(StealthTest, PiDdbCleanerValidation) {
    UNICODE_STRING mockName = {};
    EXPECT_EQ(unpd::stealth::PiDdbCleaner::CleanDriverTrace(nullptr, 0), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::stealth::PiDdbCleaner::CleanDriverTrace(&mockName, 0), STATUS_INVALID_PARAMETER);
    uint8_t buffer[32] = { 0x48, 0x8B, 0x05, 0x01, 0x02, 0x03, 0x04 };
    EXPECT_TRUE(unpd::stealth::PiDdbCleaner::IsPatternValid(buffer));
    EXPECT_FALSE(unpd::stealth::PiDdbCleaner::IsPatternValid(nullptr));
}

TEST(StealthTest, UnloadedCleanerValidation) {
    UNICODE_STRING mockName = {};
    EXPECT_EQ(unpd::stealth::UnloadedCleaner::CleanUnloadedDrivers(nullptr), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::stealth::UnloadedCleaner::CleanUnloadedDrivers(&mockName), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::stealth::UnloadedCleaner::CleanBigPoolTable(nullptr), STATUS_INVALID_PARAMETER);
}
