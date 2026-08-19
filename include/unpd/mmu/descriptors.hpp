#pragma once

#ifndef UNPD_MMU_DESCRIPTORS_HPP
#define UNPD_MMU_DESCRIPTORS_HPP

#include <stdint.h>

namespace unpd::mmu {

#pragma pack(push, 2)
/// 10-byte GDTR / IDTR pseudo-descriptor format
struct DESCRIPTOR_TABLE_REGISTER_64 {
    uint16_t Limit;
    uint64_t BaseAddress;
};
#pragma pack(pop)

static_assert(sizeof(DESCRIPTOR_TABLE_REGISTER_64) == 10, "DESCRIPTOR_TABLE_REGISTER_64 must be 10 bytes");

#pragma pack(push, 1)
/// 16-byte x86-64 IDT Entry (Interrupt / Trap Gate)
struct IDT_ENTRY_64 {
    uint16_t OffsetLow;         // Bits 0..15 of ISR entrypoint
    uint16_t Selector;          // Code segment selector (e.g. 0x10)
    uint8_t  Ist : 3;           // Interrupt Stack Table index (0..7)
    uint8_t  Reserved0 : 5;     // Must be zero
    uint8_t  Type : 4;          // Gate Type (0xE = 64-bit Interrupt, 0xF = 64-bit Trap)
    uint8_t  StorageSegment : 1;// 0 for system gates
    uint8_t  Dpl : 2;           // Descriptor Privilege Level (0 = Kernel, 3 = User)
    uint8_t  Present : 1;       // Segment Present flag
    uint16_t OffsetMiddle;      // Bits 16..31 of ISR entrypoint
    uint32_t OffsetHigh;        // Bits 32..63 of ISR entrypoint
    uint32_t Reserved1;         // Reserved, must be zero

    [[nodiscard]] constexpr uint64_t GetOffset() const noexcept {
        return static_cast<uint64_t>(OffsetLow) |
               (static_cast<uint64_t>(OffsetMiddle) << 16) |
               (static_cast<uint64_t>(OffsetHigh) << 32);
    }

    constexpr void SetOffset(uint64_t handlerAddress) noexcept {
        OffsetLow = static_cast<uint16_t>(handlerAddress & 0xFFFF);
        OffsetMiddle = static_cast<uint16_t>((handlerAddress >> 16) & 0xFFFF);
        OffsetHigh = static_cast<uint32_t>((handlerAddress >> 32) & 0xFFFFFFFF);
    }
};
#pragma pack(pop)

static_assert(sizeof(IDT_ENTRY_64) == 16, "IDT_ENTRY_64 must be exactly 16 bytes");

#pragma pack(push, 1)
/// 8-byte x86-64 Standard GDT Segment Descriptor
struct GDT_ENTRY_64 {
    uint16_t LimitLow;
    uint16_t BaseLow;
    uint8_t  BaseMiddle;
    uint8_t  Type : 4;
    uint8_t  System : 1;
    uint8_t  Dpl : 2;
    uint8_t  Present : 1;
    uint8_t  LimitHigh : 4;
    uint8_t  Available : 1;
    uint8_t  LongMode : 1;      // 1 for 64-bit code segment
    uint8_t  DefaultBig : 1;    // 0 for 64-bit code segment
    uint8_t  Granularity : 1;   // 1 = 4KB page granularity
    uint8_t  BaseHigh;

    [[nodiscard]] constexpr uint32_t GetBase() const noexcept {
        return static_cast<uint32_t>(BaseLow) |
               (static_cast<uint32_t>(BaseMiddle) << 16) |
               (static_cast<uint32_t>(BaseHigh) << 24);
    }
};
#pragma pack(pop)

static_assert(sizeof(GDT_ENTRY_64) == 8, "GDT_ENTRY_64 must be exactly 8 bytes");

#pragma pack(push, 1)
/// 104-byte x86-64 Task State Segment (TSS64)
struct TSS64 {
    uint32_t Reserved0;
    uint64_t Rsp0;              // Ring-0 Stack Pointer
    uint64_t Rsp1;              // Ring-1 Stack Pointer
    uint64_t Rsp2;              // Ring-2 Stack Pointer
    uint64_t Reserved1;
    uint64_t Ist1;              // Interrupt Stack Table 1..7
    uint64_t Ist2;
    uint64_t Ist3;
    uint64_t Ist4;
    uint64_t Ist5;
    uint64_t Ist6;
    uint64_t Ist7;
    uint64_t Reserved2;
    uint16_t Reserved3;
    uint16_t IoMapBase;         // I/O Permission Bit Map base address
};
#pragma pack(pop)

static_assert(sizeof(TSS64) == 104, "TSS64 must be exactly 104 bytes");

/// x86-64 DR7 Hardware Debug Control Register
union DR7_REGISTER_64 {
    uint64_t Value;
    struct {
        uint64_t L0 : 1;        // Local breakpoint enable 0
        uint64_t G0 : 1;        // Global breakpoint enable 0
        uint64_t L1 : 1;        // Local breakpoint enable 1
        uint64_t G1 : 1;        // Global breakpoint enable 1
        uint64_t L2 : 1;        // Local breakpoint enable 2
        uint64_t G2 : 1;        // Global breakpoint enable 2
        uint64_t L3 : 1;        // Local breakpoint enable 3
        uint64_t G3 : 1;        // Global breakpoint enable 3
        uint64_t LE : 1;        // Local exact breakpoint enable
        uint64_t GE : 1;        // Global exact breakpoint enable
        uint64_t Reserved0 : 3; // Must be 001b
        uint64_t GD : 1;        // General detect enable
        uint64_t Reserved1 : 2; // Must be 00b
        uint64_t RW0 : 2;       // Condition 0 (00=Exec, 01=Write, 11=ReadWrite)
        uint64_t LEN0 : 2;      // Length 0 (00=1B, 01=2B, 10=8B, 11=4B)
        uint64_t RW1 : 2;       // Condition 1
        uint64_t LEN1 : 2;      // Length 1
        uint64_t RW2 : 2;       // Condition 2
        uint64_t LEN2 : 2;      // Length 2
        uint64_t RW3 : 2;       // Condition 3
        uint64_t LEN3 : 2;      // Length 3
        uint64_t Reserved2 : 32;// Upper 32-bits reserved
    };
};

static_assert(sizeof(DR7_REGISTER_64) == 8, "DR7_REGISTER_64 must be 8 bytes");

/// x86-64 CR0 Control Register Bitfield
union CR0_REGISTER_64 {
    uint64_t Value;
    struct {
        uint64_t PE : 1;        // Protection Enable
        uint64_t MP : 1;        // Monitor Coprocessor
        uint64_t EM : 1;        // Emulation
        uint64_t TS : 1;        // Task Switched
        uint64_t ET : 1;        // Extension Type
        uint64_t NE : 1;        // Numeric Error
        uint64_t Reserved0 : 10;
        uint64_t WP : 1;        // Write Protect
        uint64_t Reserved1 : 1;
        uint64_t AM : 1;        // Alignment Mask
        uint64_t Reserved2 : 10;
        uint64_t NW : 1;        // Not Write-through
        uint64_t CD : 1;        // Cache Disable
        uint64_t PG : 1;        // Paging Enable
        uint64_t Reserved3 : 32;
    };
};

static_assert(sizeof(CR0_REGISTER_64) == 8, "CR0_REGISTER_64 must be 8 bytes");

/// x86-64 CR4 Control Register Bitfield
union CR4_REGISTER_64 {
    uint64_t Value;
    struct {
        uint64_t VME : 1;       // Virtual-8086 Mode Extensions
        uint64_t PVI : 1;       // Protected-Mode Virtual Interrupts
        uint64_t TSD : 1;       // Time Stamp Disable
        uint64_t DE : 1;        // Debugging Extensions
        uint64_t PSE : 1;       // Page Size Extensions
        uint64_t PAE : 1;       // Physical Address Extension
        uint64_t MCE : 1;       // Machine-Check Enable
        uint64_t PGE : 1;       // Page Global Enable
        uint64_t PCE : 1;       // Performance-Monitoring Counter Enable
        uint64_t OSFXSR : 1;    // OS Support for FXSAVE/FXRSTOR
        uint64_t OSXMMEXCPT : 1;// OS Support for Unmasked SIMD Floating-Point Exceptions
        uint64_t UMIP : 1;      // User-Mode Instruction Prevention
        uint64_t LA57 : 1;      // 57-bit Linear Addresses (5-level paging)
        uint64_t VMXE : 1;      // VMX Enable
        uint64_t SMXE : 1;      // SMX Enable
        uint64_t Reserved0 : 1;
        uint64_t FSGSBASE : 1;  // RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE instructions
        uint64_t PCIDE : 1;     // PCID Enable
        uint64_t OSXSAVE : 1;   // XSAVE and Processor Extended States Enable
        uint64_t KL : 1;        // Key Locker Enable
        uint64_t SMEP : 1;      // Supervisor Mode Execution Prevention
        uint64_t SMAP : 1;      // Supervisor Mode Access Prevention
        uint64_t PKE : 1;       // Protection Keys Enable for User Pages
        uint64_t CET : 1;       // Control-Flow Enforcement Technology
        uint64_t PKS : 1;       // Protection Keys Enable for Supervisor Pages
        uint64_t Reserved1 : 39;
    };
};

static_assert(sizeof(CR4_REGISTER_64) == 8, "CR4_REGISTER_64 must be 8 bytes");

/// IA32_EFER Extended Feature Enable Register
union IA32_EFER_REGISTER_64 {
    uint64_t Value;
    struct {
        uint64_t SCE : 1;       // SYSCALL/SYSRET Enable
        uint64_t Reserved0 : 7;
        uint64_t LME : 1;       // Long Mode Enable
        uint64_t Reserved1 : 1;
        uint64_t LMA : 1;       // Long Mode Active
        uint64_t NXE : 1;       // No-Execute Enable
        uint64_t SVME : 1;      // Secure Virtual Machine Enable (AMD)
        uint64_t LMSLE : 1;     // Long Mode Segment Limit Enable
        uint64_t FFXSR : 1;     // Fast FXSAVE/FXRSTOR
        uint64_t TCE : 1;       // Translation Cache Extension
        uint64_t Reserved2 : 48;
    };
};

static_assert(sizeof(IA32_EFER_REGISTER_64) == 8, "IA32_EFER_REGISTER_64 must be 8 bytes");

/// IA32_PAT Page Attribute Table Register
union IA32_PAT_REGISTER_64 {
    uint64_t Value;
    struct {
        uint64_t PA0 : 3;       // Memory type for PAT entry 0 (Default WB)
        uint64_t Reserved0 : 5;
        uint64_t PA1 : 3;       // PAT entry 1 (Default WT)
        uint64_t Reserved1 : 5;
        uint64_t PA2 : 3;       // PAT entry 2 (Default UC-)
        uint64_t Reserved2 : 5;
        uint64_t PA3 : 3;       // PAT entry 3 (Default UC)
        uint64_t Reserved3 : 5;
        uint64_t PA4 : 3;       // PAT entry 4 (Default WB)
        uint64_t Reserved4 : 5;
        uint64_t PA5 : 3;       // PAT entry 5 (Default WT)
        uint64_t Reserved5 : 5;
        uint64_t PA6 : 3;       // PAT entry 6 (Default UC-)
        uint64_t Reserved6 : 5;
        uint64_t PA7 : 3;       // PAT entry 7 (Default UC)
        uint64_t Reserved7 : 5;
    };
};

static_assert(sizeof(IA32_PAT_REGISTER_64) == 8, "IA32_PAT_REGISTER_64 must be 8 bytes");

} // namespace unpd::mmu

#endif // UNPD_MMU_DESCRIPTORS_HPP
