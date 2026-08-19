#include <gtest/gtest.h>
#include <unpd/stealth/piddb_cleaner.hpp>
#include <unpd/stealth/unloaded_cleaner.hpp>

TEST(StealthTest, PiDdbCleanerValidation) {
    UNICODE_STRING mockName = {};
    EXPECT_EQ(unpd::stealth::PiDdbCleaner::CleanDriverTrace(nullptr, 0), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::stealth::PiDdbCleaner::CleanDriverTrace(&mockName, 0), STATUS_INVALID_PARAMETER);
    EXPECT_TRUE(unpd::stealth::PiDdbCleaner::IsPatternValid((void*)0x12345));
}

TEST(StealthTest, UnloadedCleanerValidation) {
    UNICODE_STRING mockName = {};
    EXPECT_EQ(unpd::stealth::UnloadedCleaner::CleanUnloadedDrivers(nullptr), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::stealth::UnloadedCleaner::CleanUnloadedDrivers(&mockName), STATUS_INVALID_PARAMETER);
    EXPECT_EQ(unpd::stealth::UnloadedCleaner::CleanBigPoolTable(nullptr), STATUS_INVALID_PARAMETER);
}
