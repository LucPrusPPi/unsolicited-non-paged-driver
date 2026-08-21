#include <gtest/gtest.h>
#include <unpd/common.h>
#include <unpd/config.hpp>
#include <unpd/mmu/physical_memory.hpp>
#include <unpd/mmu/vmt_resolver.hpp>
#include <unpd/comm/shared_memory.hpp>
#include <unpd/simd/simd_engine.hpp>
#include "emulator/virtual_mmu.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <random>

using namespace unpd::test::emulator;
using namespace unpd::comm;

// ============================================================================
// 1. Concurrency & Contention Stress Tests: Lockless SPSC Ring Pipeline
// ============================================================================
TEST(ArchitectureDeepStress, LocklessRing_SPSC_HighThroughputBurst) {
    std::vector<uint8_t> channelBuffer(sizeof(unpd::comm::SharedChannelHeader) + 16384, 0);
    PVOID channelVa = channelBuffer.data();
    ASSERT_EQ(unpd::comm::SharedMemoryChannel::Initialize(channelVa, channelBuffer.size()), STATUS_SUCCESS);

    constexpr int kTotalPackets = 50000;
    std::atomic<bool> startFlag{false};
    std::atomic<uint64_t> totalPushed{0};
    std::atomic<uint64_t> totalPopped{0};

    // Client Producer Thread
    std::thread producer([&]() {
        while (!startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (int i = 0; i < kTotalPackets; ++i) {
            unpd::comm::SharedCommand cmd{};
            cmd.Magic = SHARED_MEM_MAGIC_REQ;
            cmd.Opcode = UNPD_OPCODE_PING;
            cmd.Sequence = static_cast<uint32_t>(i + 1);
            cmd.PayloadSize = 0;

            while (!unpd::comm::SharedMemoryChannel::PushCommand(channelVa, cmd)) {
                std::this_thread::yield();
            }
            totalPushed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Server / Driver Worker Thread
    std::thread serverWorker([&]() {
        while (!startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        while (totalPopped.load(std::memory_order_relaxed) < kTotalPackets) {
            unpd::comm::SharedMemoryChannel::PollAndDispatch(channelVa);

            unpd::comm::SharedResponse resp{};
            while (unpd::comm::SharedMemoryChannel::PopResponse(channelVa, resp)) {
                EXPECT_EQ(resp.Magic, SHARED_MEM_MAGIC_RESP);
                EXPECT_EQ(resp.Opcode, UNPD_OPCODE_PING);
                totalPopped.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::yield();
        }
    });

    startFlag.store(true, std::memory_order_release);

    producer.join();
    serverWorker.join();

    EXPECT_EQ(totalPushed.load(), kTotalPackets);
    EXPECT_EQ(totalPopped.load(), kTotalPackets);
}

// ============================================================================
// 2. Hardware MMU Paging & Translation Stress Tests (Multi-Level Edge Cases)
// ============================================================================
TEST(ArchitectureDeepStress, VirtualMmu_ExhaustiveWalk_WithTLBThrashing) {
    VirtualMmu mmu;
    const uint64_t cr3 = mmu.CreatePml4();
    ASSERT_NE(cr3, 0ULL);

    constexpr size_t kPageCount = 256;
    std::vector<uint64_t> virtualAddresses;
    std::vector<uint64_t> physicalAddresses;

    // Map 256 random sparse 4KB pages across distinct canonical PML4/PDPT/PD/PT tables
    std::mt19937_64 rng(0x1337BEEF);
    for (size_t i = 0; i < kPageCount; ++i) {
        uint64_t va = (0x0000700000000000ULL | (rng() & 0x00000FFFFFFFF000ULL));
        uint64_t pa = mmu.AllocatePhysicalPage();
        ASSERT_NE(pa, 0ULL);

        ASSERT_TRUE(mmu.MapPage(cr3, va, pa, PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor));

        // Write deterministic tag into physical page
        uint64_t tag = va ^ 0xA5A5A5A55A5A5A5AULL;
        ASSERT_TRUE(mmu.WritePhysical(pa, &tag, sizeof(tag)));

        virtualAddresses.push_back(va);
        physicalAddresses.push_back(pa);
    }

    // Exhaustive sequential translation and verification across all 256 virtual pages
    for (size_t i = 0; i < kPageCount; ++i) {
        uint64_t va = virtualAddresses[i];
        uint64_t expectedPa = physicalAddresses[i];

        auto trans = mmu.Translate(cr3, va, false, false, false);
        ASSERT_TRUE(trans.has_value()) << "Translation failed for VA: " << std::hex << va;
        EXPECT_EQ(trans.value(), expectedPa) << "Mismatch PA for VA: " << std::hex << va;

        uint64_t readTag = 0;
        size_t bytesRead = 0;
        ASSERT_TRUE(mmu.ReadVirtual(cr3, va, &readTag, sizeof(readTag), &bytesRead));
        EXPECT_EQ(bytesRead, sizeof(readTag));
        EXPECT_EQ(readTag, (va ^ 0xA5A5A5A55A5A5A5AULL));
    }

    // Verify TLB hits on second traversal
    size_t initialHits = mmu.GetTlbHits();
    for (size_t i = 0; i < 64; ++i) {
        uint64_t va = virtualAddresses[kPageCount - 1 - i];
        auto trans = mmu.Translate(cr3, va, false, false, false);
        ASSERT_TRUE(trans.has_value());
    }
    EXPECT_GT(mmu.GetTlbHits(), initialHits);
}

// ============================================================================
// 3. Physical Memory Security Boundary Matrix Validation
// ============================================================================
TEST(ArchitectureDeepStress, PhysicalMemory_SecurityBoundaries) {
    // 1. Lower 1MB BIOS / IVT / BDA block
    EXPECT_FALSE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0x00000000ULL, 4096));
    EXPECT_FALSE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0x00000500ULL, 64));
    EXPECT_FALSE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0x0009FFFFULL, 4096));

    // 2. APIC / IO-APIC / PCI MMIO Range (0xFEE00000 .. 0xFFFFFFFF)
    EXPECT_FALSE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0xFEE00000ULL, 4096)); // Local APIC
    EXPECT_FALSE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0xFEC00000ULL, 4096)); // IO-APIC
    EXPECT_FALSE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0xFFFF0000ULL, 4096)); // BIOS ROM

    // 3. Valid System Physical RAM Ranges
    EXPECT_TRUE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0x00100000ULL, 4096));  // 1MB + 1 page
    EXPECT_TRUE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0x100000000ULL, 4096)); // > 4GB RAM
    EXPECT_TRUE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0x200000000ULL, 65536));

    // 4. Zero address and Zero Size checks
    EXPECT_FALSE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0, 0));
    EXPECT_FALSE(unpd::mmu::PhysicalMemory::IsPhysicalAddressAllowed(0x100000000ULL, 0));
}

// ============================================================================
// 4. SIMD Engine Stress: AVX2 / AVX-512 Mask & Unaligned Matrix
// ============================================================================
TEST(ArchitectureDeepStress, SimdEngine_ExhaustivePatternOffsetSearch) {
    constexpr size_t kBufSize = 4096;
    alignas(64) uint8_t buffer[kBufSize];
    memset(buffer, 0x90, sizeof(buffer));

    const uint8_t needle[] = { 0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0xC3 };
    const char mask[] = "xxx????x";

    // Test needle match at every single offset from 0 to 4088 (verifying unaligned SIMD shifts)
    for (size_t offset = 0; offset <= kBufSize - sizeof(needle); ++offset) {
        // Place needle
        memcpy(buffer + offset, needle, sizeof(needle));
        // Fill wildcard bytes with random junk
        buffer[offset + 3] = static_cast<uint8_t>(offset & 0xFF);
        buffer[offset + 4] = static_cast<uint8_t>((offset >> 8) & 0xFF);
        buffer[offset + 5] = static_cast<uint8_t>(offset ^ 0xAA);
        buffer[offset + 6] = static_cast<uint8_t>(offset ^ 0x55);

        const void* match = unpd::simd::SimdEngine::ScanPattern(buffer, sizeof(buffer), needle, mask);
        ASSERT_NE(match, nullptr) << "Failed to find pattern at offset: " << offset;
        EXPECT_EQ(match, buffer + offset) << "Incorrect match address at offset: " << offset;

        // Restore buffer
        memset(buffer + offset, 0x90, sizeof(needle));
    }
}
