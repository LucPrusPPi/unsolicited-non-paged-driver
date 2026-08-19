#include <gtest/gtest.h>
#include <unpd/common.h>
#include <unpd/mmu/paging_types.hpp>
#include <unpd/mmu/descriptors.hpp>
#include <unpd/kstd/expected.hpp>
#include "emulator/virtual_mmu.hpp"

using namespace unpd::test::emulator;

TEST(MmuAdvancedTest, VirtualMemoryWalkEndToEnd) {
    VirtualMmu mmu;
    const uint64_t cr3 = mmu.CreatePml4();
    ASSERT_NE(cr3, 0ULL);

    const uint64_t testVa = 0x00007FFF80001000ULL;
    const uint64_t testPa = mmu.AllocatePhysicalPage();
    ASSERT_NE(testPa, 0ULL);

    EXPECT_TRUE(mmu.MapPage(cr3, testVa, testPa, PageFlags::Present | PageFlags::ReadWrite));

    // Write a test sequence to physical memory
    const uint32_t magic = 0xDEADBEEF;
    EXPECT_TRUE(mmu.WritePhysical(testPa, &magic, sizeof(magic)));

    // Read back through virtual translation
    uint32_t readMagic = 0;
    size_t bytesRead = 0;
    EXPECT_TRUE(mmu.ReadVirtual(cr3, testVa, &readMagic, sizeof(readMagic), &bytesRead));
    EXPECT_EQ(bytesRead, sizeof(magic));
    EXPECT_EQ(readMagic, magic);
}

TEST(MmuAdvancedTest, VirtualPteRemapping_PermissionsChange) {
    VirtualMmu mmu;
    const uint64_t cr3 = mmu.CreatePml4();
    ASSERT_NE(cr3, 0ULL);

    const uint64_t testVa = 0x00007FFF90000000ULL;
    const uint64_t testPa = mmu.AllocatePhysicalPage();
    ASSERT_NE(testPa, 0ULL);

    // Initial mapping: Read-only
    EXPECT_TRUE(mmu.MapPage(cr3, testVa, testPa, PageFlags::Present));

    // Verify Write triggers #PF
    auto writeTrans1 = mmu.Translate(cr3, testVa, true, false, false);
    EXPECT_FALSE(writeTrans1.has_value());

    // Remap / update PTE flags to ReadWrite
    EXPECT_TRUE(mmu.SetPageFlags(cr3, testVa, PageFlags::Present | PageFlags::ReadWrite));

    // Verify Write now succeeds
    auto writeTrans2 = mmu.Translate(cr3, testVa, true, false, false);
    EXPECT_TRUE(writeTrans2.has_value());
    EXPECT_EQ(writeTrans2.value(), testPa);
}
