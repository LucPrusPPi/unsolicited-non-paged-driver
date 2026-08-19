#include <gtest/gtest.h>
#include "emulator/virtual_mmu.hpp"
#include <vector>
#include <string>

using namespace unpd::test::emulator;

class VirtualMmuTest : public ::testing::Test {
protected:
    VirtualMmu mmu;
    uint64_t cr3 = 0;

    void SetUp() override {
        cr3 = mmu.CreatePml4();
        ASSERT_NE(cr3, 0ULL);
    }
};

TEST_F(VirtualMmuTest, Basic4KbPageTranslation) {
    const uint64_t va = 0x00007FFF12345678ULL;
    const uint64_t pa = mmu.AllocatePhysicalPage();
    ASSERT_NE(pa, 0ULL);

    EXPECT_TRUE(mmu.MapPage(cr3, va, pa, PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor));

    auto translation = mmu.Translate(cr3, va, false, true, false);
    ASSERT_TRUE(translation.has_value());
    EXPECT_EQ(translation.value(), pa + 0x678ULL);
}

TEST_F(VirtualMmuTest, Huge1GbPageTranslation) {
    const uint64_t va = 0x00007FFF40000000ULL; // 1GB aligned base
    const uint64_t pa1Gb = 0x0000000040000000ULL; // 1GB physical base

    EXPECT_TRUE(mmu.MapHugePage1GB(cr3, va, pa1Gb, PageFlags::Present | PageFlags::ReadWrite));

    const uint64_t testOffset = 0x12345678ULL;
    auto translation = mmu.Translate(cr3, va + testOffset, false, false, false);
    ASSERT_TRUE(translation.has_value());
    EXPECT_EQ(translation.value(), pa1Gb + testOffset);
}

TEST_F(VirtualMmuTest, Large2MbPageTranslation) {
    const uint64_t va = 0x00007FFF00200000ULL; // 2MB aligned
    const uint64_t pa2Mb = 0x0000000000200000ULL;

    EXPECT_TRUE(mmu.MapLargePage2MB(cr3, va, pa2Mb, PageFlags::Present | PageFlags::ReadWrite));

    const uint64_t testOffset = 0x00054321ULL;
    auto translation = mmu.Translate(cr3, va + testOffset, false, false, false);
    ASSERT_TRUE(translation.has_value());
    EXPECT_EQ(translation.value(), pa2Mb + testOffset);
}

TEST_F(VirtualMmuTest, PageFaultNotPresent) {
    const uint64_t unmappedVa = 0x0000123456789000ULL;
    auto translation = mmu.Translate(cr3, unmappedVa, false, false, false);
    ASSERT_FALSE(translation.has_value());
    EXPECT_EQ(translation.error().ErrorCode.Present, 0);
    EXPECT_EQ(translation.error().FaultingVirtualAddress, unmappedVa);
}

TEST_F(VirtualMmuTest, PageFaultWriteProtect) {
    const uint64_t va = 0x00007FFF00001000ULL;
    const uint64_t pa = mmu.AllocatePhysicalPage();
    ASSERT_NE(pa, 0ULL);

    // Read-only page
    EXPECT_TRUE(mmu.MapPage(cr3, va, pa, PageFlags::Present));

    // Read succeeds
    auto readTrans = mmu.Translate(cr3, va, false, false, false);
    EXPECT_TRUE(readTrans.has_value());

    // Write fails with #PF (Present = 1, Write = 1)
    auto writeTrans = mmu.Translate(cr3, va, true, false, false);
    ASSERT_FALSE(writeTrans.has_value());
    EXPECT_EQ(writeTrans.error().ErrorCode.Present, 1);
    EXPECT_EQ(writeTrans.error().ErrorCode.Write, 1);
}

TEST_F(VirtualMmuTest, PageFaultNoExecute) {
    const uint64_t va = 0x00007FFF00002000ULL;
    const uint64_t pa = mmu.AllocatePhysicalPage();
    ASSERT_NE(pa, 0ULL);

    EXPECT_TRUE(mmu.MapPage(cr3, va, pa, PageFlags::Present | PageFlags::ReadWrite | PageFlags::NoExecute));

    // Read/write succeed
    EXPECT_TRUE(mmu.Translate(cr3, va, false, false, false).has_value());

    // Instruction fetch fails with NX fault
    auto fetchTrans = mmu.Translate(cr3, va, false, false, true);
    ASSERT_FALSE(fetchTrans.has_value());
    EXPECT_EQ(fetchTrans.error().ErrorCode.Present, 1);
    EXPECT_EQ(fetchTrans.error().ErrorCode.InstructionFetch, 1);
}

TEST_F(VirtualMmuTest, NonCanonicalAddressFault) {
    const uint64_t nonCanonicalVa = 0x00F07FFF00001000ULL; // Bits 48-63 not matching bit 47
    auto translation = mmu.Translate(cr3, nonCanonicalVa, false, false, false);
    ASSERT_FALSE(translation.has_value());
    EXPECT_EQ(translation.error().FaultLevel, 4);
}

TEST_F(VirtualMmuTest, VirtualReadWriteMultiPageChunking) {
    const uint64_t vaBase = 0x00007FFF10000000ULL;
    const uint64_t pa1 = mmu.AllocatePhysicalPage();
    const uint64_t pa2 = mmu.AllocatePhysicalPage();
    ASSERT_NE(pa1, 0ULL);
    ASSERT_NE(pa2, 0ULL);

    EXPECT_TRUE(mmu.MapPage(cr3, vaBase, pa1, PageFlags::Present | PageFlags::ReadWrite));
    EXPECT_TRUE(mmu.MapPage(cr3, vaBase + 4096, pa2, PageFlags::Present | PageFlags::ReadWrite));

    // Write spanning across page boundary (starts at offset 4000, length 200 bytes)
    std::string testPayload(200, 'Z');
    testPayload[0] = 'S';
    testPayload[199] = 'E';

    size_t written = 0;
    EXPECT_TRUE(mmu.WriteVirtual(cr3, vaBase + 4000, testPayload.data(), testPayload.size(), &written));
    EXPECT_EQ(written, 200);

    std::string readBuffer(200, '\0');
    size_t readCount = 0;
    EXPECT_TRUE(mmu.ReadVirtual(cr3, vaBase + 4000, readBuffer.data(), readBuffer.size(), &readCount));
    EXPECT_EQ(readCount, 200);
    EXPECT_EQ(readBuffer, testPayload);
}

TEST_F(VirtualMmuTest, TlbHitAndInvlpgVerification) {
    const uint64_t va = 0x00007FFF20000000ULL;
    const uint64_t pa = mmu.AllocatePhysicalPage();
    ASSERT_NE(pa, 0ULL);

    EXPECT_TRUE(mmu.MapPage(cr3, va, pa, PageFlags::Present | PageFlags::ReadWrite));

    mmu.FlushTlb();
    size_t initialMisses = mmu.GetTlbMisses();
    size_t initialHits = mmu.GetTlbHits();

    // 1st translation: TLB Miss
    auto trans1 = mmu.Translate(cr3, va, false, false, false);
    EXPECT_TRUE(trans1.has_value());
    EXPECT_EQ(mmu.GetTlbMisses(), initialMisses + 1);

    // 2nd translation: TLB Hit
    auto trans2 = mmu.Translate(cr3, va, false, false, false);
    EXPECT_TRUE(trans2.has_value());
    EXPECT_EQ(mmu.GetTlbHits(), initialHits + 1);

    // Invalidate via Invlpg
    mmu.Invlpg(va);

    // 3rd translation: TLB Miss again
    auto trans3 = mmu.Translate(cr3, va, false, false, false);
    EXPECT_TRUE(trans3.has_value());
    EXPECT_EQ(mmu.GetTlbMisses(), initialMisses + 2);
}

TEST_F(VirtualMmuTest, HigherHalfCanonicalTranslation) {
    const uint64_t kernelVa = 0xFFFF888012345000ULL;
    const uint64_t pa = mmu.AllocatePhysicalPage();
    ASSERT_NE(pa, 0ULL);

    EXPECT_TRUE(mmu.MapPage(cr3, kernelVa, pa, PageFlags::Present | PageFlags::ReadWrite));

    auto translation = mmu.Translate(cr3, kernelVa, false, false, false);
    ASSERT_TRUE(translation.has_value());
    EXPECT_EQ(translation.value(), pa);
}

TEST_F(VirtualMmuTest, UserSupervisorPrivilegeCheck) {
    const uint64_t supervisorVa = 0x00007FFF30000000ULL;
    const uint64_t pa = mmu.AllocatePhysicalPage();
    ASSERT_NE(pa, 0ULL);

    // Map supervisor-only (without UserSupervisor flag)
    EXPECT_TRUE(mmu.MapPage(cr3, supervisorVa, pa, PageFlags::Present | PageFlags::ReadWrite));

    // Kernel mode read succeeds
    auto kernelTrans = mmu.Translate(cr3, supervisorVa, false, false, false);
    EXPECT_TRUE(kernelTrans.has_value());

    // User mode read triggers #PF protection fault
    auto userTrans = mmu.Translate(cr3, supervisorVa, false, true, false);
    ASSERT_FALSE(userTrans.has_value());
    EXPECT_EQ(userTrans.error().ErrorCode.Present, 1);
    EXPECT_EQ(userTrans.error().ErrorCode.User, 1);
}

TEST_F(VirtualMmuTest, MultiPageContiguousBufferChunking) {
    const uint64_t vaStart = 0x00007FFF50000000ULL;
    const size_t numPages = 3;
    std::vector<uint64_t> pas;
    for (size_t i = 0; i < numPages; ++i) {
        uint64_t pa = mmu.AllocatePhysicalPage();
        ASSERT_NE(pa, 0ULL);
        pas.push_back(pa);
        EXPECT_TRUE(mmu.MapPage(cr3, vaStart + (i * 4096), pa, PageFlags::Present | PageFlags::ReadWrite));
    }

    // Write a 10,000 byte buffer that spans across all 3 pages
    std::vector<uint8_t> sendData(10000);
    for (size_t i = 0; i < sendData.size(); ++i) {
        sendData[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);
    }

    size_t written = 0;
    EXPECT_TRUE(mmu.WriteVirtual(cr3, vaStart + 500, sendData.data(), sendData.size(), &written));
    EXPECT_EQ(written, sendData.size());

    std::vector<uint8_t> recvData(10000, 0);
    size_t bytesRead = 0;
    EXPECT_TRUE(mmu.ReadVirtual(cr3, vaStart + 500, recvData.data(), recvData.size(), &bytesRead));
    EXPECT_EQ(bytesRead, sendData.size());
    EXPECT_EQ(sendData, recvData);
}
