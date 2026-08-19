#pragma once

#ifndef UNPD_TESTS_EMULATOR_VIRTUAL_MMU_HPP
#define UNPD_TESTS_EMULATOR_VIRTUAL_MMU_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <optional>
#include <cstring>
#include <unpd/kstd/expected.hpp>

namespace unpd::test::emulator {

// ============================================================================
// Page Fault Exception Codes & Flags (x86-64 Architectural Model)
// ============================================================================
struct PageFaultCode {
    uint32_t Present : 1;            // Bit 0: 0 = Not-Present, 1 = Protection Violation
    uint32_t Write : 1;              // Bit 1: 0 = Read Access, 1 = Write Access
    uint32_t User : 1;               // Bit 2: 0 = Supervisor Mode, 1 = User Mode
    uint32_t ReservedViolation : 1;  // Bit 3: Reserved bit set in paging hierarchy
    uint32_t InstructionFetch : 1;   // Bit 4: NX / Instruction fetch violation
    uint32_t Reserved : 27;

    [[nodiscard]] uint32_t AsU32() const noexcept {
        uint32_t val = 0;
        std::memcpy(&val, this, sizeof(val));
        return val;
    }
};

struct PageFaultException {
    uint64_t FaultingVirtualAddress = 0;
    PageFaultCode ErrorCode{};
    uint8_t FaultLevel = 0; // 4 = PML4, 3 = PDPT, 2 = PD, 1 = PT
};

// ============================================================================
// Paging Entry Bit Flags
// ============================================================================
namespace PageFlags {
    inline constexpr uint64_t Present         = (1ULL << 0);
    inline constexpr uint64_t ReadWrite       = (1ULL << 1);
    inline constexpr uint64_t UserSupervisor  = (1ULL << 2);
    inline constexpr uint64_t PageWriteThrough= (1ULL << 3);
    inline constexpr uint64_t PageCacheDisable= (1ULL << 4);
    inline constexpr uint64_t Accessed        = (1ULL << 5);
    inline constexpr uint64_t Dirty           = (1ULL << 6);
    inline constexpr uint64_t LargePage       = (1ULL << 7); // PS bit in PDPT (1GB) or PD (2MB)
    inline constexpr uint64_t Global          = (1ULL << 8);
    inline constexpr uint64_t NoExecute       = (1ULL << 63);
}

// ============================================================================
// Translation Lookaside Buffer (TLB) Simulation Entry
// ============================================================================
struct TlbEntry {
    bool Valid = false;
    uint64_t Cr3 = 0;
    uint64_t VirtualPage = 0;
    uint64_t PhysicalPage = 0;
    uint64_t Flags = 0;
    uint64_t AccessTimestamp = 0;
};

// ============================================================================
// Virtual x86-64 MMU Sandbox Class
// ============================================================================
class VirtualMmu {
public:
    static constexpr size_t DEFAULT_RAM_SIZE = 64 * 1024 * 1024; // 64 MB
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t LARGE_PAGE_SIZE = 2 * 1024 * 1024;   // 2 MB
    static constexpr size_t HUGE_PAGE_SIZE  = 1024 * 1024 * 1024;// 1 GB
    static constexpr size_t TLB_CAPACITY = 64;

    explicit VirtualMmu(size_t ramSize = DEFAULT_RAM_SIZE);
    ~VirtualMmu() = default;

    // Disallow copies
    VirtualMmu(const VirtualMmu&) = delete;
    VirtualMmu& operator=(const VirtualMmu&) = delete;
    VirtualMmu(VirtualMmu&&) noexcept = default;
    VirtualMmu& operator=(VirtualMmu&&) noexcept = default;

    // PFN Allocation & Physical Memory Operations
    [[nodiscard]] uint64_t AllocatePhysicalPage();
    [[nodiscard]] uint8_t* GetPhysicalPointer(uint64_t physicalAddress);
    [[nodiscard]] const uint8_t* GetPhysicalPointer(uint64_t physicalAddress) const;
    [[nodiscard]] bool ReadPhysical(uint64_t physicalAddress, void* buffer, size_t size) const;
    [[nodiscard]] bool WritePhysical(uint64_t physicalAddress, const void* buffer, size_t size);

    // Page Table Setup Helpers
    [[nodiscard]] uint64_t CreatePml4();
    bool MapPage(uint64_t cr3, uint64_t virtualAddress, uint64_t physicalAddress, uint64_t flags = (PageFlags::Present | PageFlags::ReadWrite));
    bool MapLargePage2MB(uint64_t cr3, uint64_t virtualAddress, uint64_t physicalAddress2MB, uint64_t flags = (PageFlags::Present | PageFlags::ReadWrite));
    bool MapHugePage1GB(uint64_t cr3, uint64_t virtualAddress, uint64_t physicalAddress1GB, uint64_t flags = (PageFlags::Present | PageFlags::ReadWrite));
    bool SetPageFlags(uint64_t cr3, uint64_t virtualAddress, uint64_t flags);

    // MMU Translation Engine (Bitwise 4-Level x86-64 Hardware Model)
    [[nodiscard]] unpd::kstd::expected<uint64_t, PageFaultException> Translate(
        uint64_t cr3,
        uint64_t virtualAddress,
        bool isWrite = false,
        bool isUser = false,
        bool isInstructionFetch = false
    );

    // Virtual Address I/O
    bool ReadVirtual(uint64_t cr3, uint64_t virtualAddress, void* buffer, size_t size, size_t* bytesRead = nullptr);
    bool WriteVirtual(uint64_t cr3, uint64_t virtualAddress, const void* buffer, size_t size, size_t* bytesWritten = nullptr);

    // TLB Management
    void Invlpg(uint64_t virtualAddress);
    void FlushTlb();
    [[nodiscard]] size_t GetTlbHits() const noexcept { return m_tlbHits; }
    [[nodiscard]] size_t GetTlbMisses() const noexcept { return m_tlbMisses; }

    // CPU Control Register Configuration
    void SetWriteProtect(bool enabled) noexcept { m_cr0_wp = enabled; }
    void SetNoExecuteEnabled(bool enabled) noexcept { m_efer_nxe = enabled; }
    void SetSmepEnabled(bool enabled) noexcept { m_cr4_smep = enabled; }
    void SetSmapEnabled(bool enabled) noexcept { m_cr4_smap = enabled; }

private:
    std::vector<uint8_t> m_physicalRam;
    uint64_t m_nextPfn = 1; // PFN 0 reserved (Null physical address)
    size_t m_maxPfn = 0;

    // CPU Flags
    bool m_cr0_wp = true;       // CR0.WP (Write Protect)
    bool m_efer_nxe = true;     // EFER.NXE (No-Execute Enable)
    bool m_cr4_smep = false;    // CR4.SMEP (Supervisor Mode Execution Prevention)
    bool m_cr4_smap = false;    // CR4.SMAP (Supervisor Mode Access Prevention)

    // TLB State
    std::array<TlbEntry, TLB_CAPACITY> m_tlb{};
    uint64_t m_tlbClock = 0;
    size_t m_tlbHits = 0;
    size_t m_tlbMisses = 0;

    [[nodiscard]] static constexpr bool IsCanonical(uint64_t va) noexcept {
        const uint64_t highBits = va >> 47;
        return (highBits == 0) || (highBits == 0x1FFFF);
    }
};

} // namespace unpd::test::emulator

#endif // UNPD_TESTS_EMULATOR_VIRTUAL_MMU_HPP
