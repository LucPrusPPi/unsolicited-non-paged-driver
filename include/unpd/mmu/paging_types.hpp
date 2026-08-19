#pragma once

#ifndef UNPD_PAGING_TYPES_HPP
#define UNPD_PAGING_TYPES_HPP

#include <stdint.h>

#pragma pack(push, 1)

namespace unpd::mmu {

// ============================================================================
// Page Sizing and Alignment Constants
// ============================================================================
constexpr uint64_t PAGE_SIZE_4KB    = 4096ULL;
constexpr uint64_t PAGE_SIZE_2MB    = 2ULL * 1024ULL * 1024ULL;
constexpr uint64_t PAGE_SIZE_1GB    = 1024ULL * 1024ULL * 1024ULL;

constexpr uint32_t PAGE_SHIFT_4KB   = 12;
constexpr uint32_t PAGE_SHIFT_2MB   = 21;
constexpr uint32_t PAGE_SHIFT_1GB   = 30;

constexpr uint64_t PAGE_MASK_4KB    = ~(PAGE_SIZE_4KB - 1ULL);
constexpr uint64_t PAGE_MASK_2MB    = ~(PAGE_SIZE_2MB - 1ULL);
constexpr uint64_t PAGE_MASK_1GB    = ~(PAGE_SIZE_1GB - 1ULL);

constexpr uint64_t PFN_MASK         = 0x000FFFFFFFFFF000ULL;

// ============================================================================
// Alignment Helpers
// ============================================================================
template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, uint64_t alignment) noexcept {
    return static_cast<T>((static_cast<uint64_t>(value) + alignment - 1ULL) & ~(alignment - 1ULL));
}

template <typename T>
[[nodiscard]] constexpr T AlignDown(T value, uint64_t alignment) noexcept {
    return static_cast<T>(static_cast<uint64_t>(value) & ~(alignment - 1ULL));
}

template <typename T>
[[nodiscard]] constexpr bool IsAligned(T value, uint64_t alignment) noexcept {
    return (static_cast<uint64_t>(value) & (alignment - 1ULL)) == 0;
}

// ============================================================================
// x86-64 Canonical 4-Level Linear Virtual Address Structure (48-bit VA)
// ============================================================================
union VIRTUAL_ADDRESS_64 {
    uint64_t Value;
    void* Pointer;
    struct {
        uint64_t Offset4KB     : 12; // [0..11]   4KB Page Offset
        uint64_t PtIndex       : 9;  // [12..20]  Page Table Index (PTE)
        uint64_t PdIndex       : 9;  // [21..29]  Page Directory Index (PDE)
        uint64_t PdptIndex     : 9;  // [30..38]  Page Directory Pointer Table (PDPTE)
        uint64_t Pml4Index     : 9;  // [39..47]  Page Map Level 4 Index (PML4E)
        uint64_t SignExtension : 16; // [48..63]  Canonical sign extension
    };
    struct {
        uint64_t Offset2MB     : 21; // [0..20]   2MB Large Page Offset
        uint64_t PdIndex2MB    : 9;  // [21..29]  Page Directory Index
        uint64_t PdptIndex2MB  : 9;  // [30..38]  Page Directory Pointer Table Index
        uint64_t Pml4Index2MB  : 9;  // [39..47]  PML4 Index
        uint64_t SignExt2MB    : 16; // [48..63]  Sign extension
    };
    struct {
        uint64_t Offset1GB     : 30; // [0..29]   1GB Huge Page Offset
        uint64_t PdptIndex1GB  : 9;  // [30..38]  PDPTE Index
        uint64_t Pml4Index1GB  : 9;  // [39..47]  PML4 Index
        uint64_t SignExt1GB    : 16; // [48..63]  Sign extension
    };
};

// ============================================================================
// CR3 Register (Paging Base and PCID)
// ============================================================================
union CR3_REGISTER_64 {
    uint64_t Value;
    struct {
        uint64_t Pcid                : 12; // [0..11]   Process Context Identifier (if enabled)
        uint64_t Pml4PhysicalAddress : 40; // [12..51]  Page Map Level 4 Physical Base Address
        uint64_t Reserved            : 12; // [52..63]  Reserved MBZ
    };
    struct {
        uint64_t ReservedFlags       : 3;  // [0..2]    MBZ
        uint64_t PageWriteThrough    : 1;  // [3]       PWT: Page-level Write-Through
        uint64_t PageCacheDisable    : 1;  // [4]       PCD: Page-level Cache Disable
        uint64_t ReservedFlags2      : 7;  // [5..11]   MBZ
        uint64_t Address             : 40; // [12..51]  PML4 Base
        uint64_t Reserved2           : 12; // [52..63]  MBZ
    };
};

// ============================================================================
// PML4 Entry (Level 4 Page Map)
// ============================================================================
union PML4_ENTRY_64 {
    uint64_t Value;
    struct {
        uint64_t Present             : 1;  // [0]       1 = Present in physical memory
        uint64_t ReadWrite           : 1;  // [1]       0 = Read-Only, 1 = Read/Write
        uint64_t UserSupervisor      : 1;  // [2]       0 = Ring 0-2 (Supervisor), 1 = Ring 3 (User)
        uint64_t PageWriteThrough    : 1;  // [3]       1 = Write-through caching
        uint64_t PageCacheDisable    : 1;  // [4]       1 = Caching disabled
        uint64_t Accessed            : 1;  // [5]       1 = Accessed by CPU
        uint64_t Ignored1            : 1;  // [6]       Ignored
        uint64_t ReservedZero        : 1;  // [7]       Must be 0
        uint64_t Ignored2            : 4;  // [8..11]   Available for OS use
        uint64_t PageFrameNumber     : 40; // [12..51]  Physical address of PDPT >> 12
        uint64_t Ignored3            : 11; // [52..62]  Available for OS use
        uint64_t ExecuteDisable      : 1;  // [63]      XD/NX: 1 = Instruction fetch prohibited (DEP)
    };
};

// ============================================================================
// PDPT Entry (Level 3 Page Directory Pointer Table)
// ============================================================================
union PDPT_ENTRY_64 {
    uint64_t Value;
    // 4KB/2MB Mapping pointing to Page Directory
    struct {
        uint64_t Present             : 1;  // [0]       Present
        uint64_t ReadWrite           : 1;  // [1]       Read/Write
        uint64_t UserSupervisor      : 1;  // [2]       User/Supervisor
        uint64_t PageWriteThrough    : 1;  // [3]       Write-through
        uint64_t PageCacheDisable    : 1;  // [4]       Cache disable
        uint64_t Accessed            : 1;  // [5]       Accessed
        uint64_t Ignored1            : 1;  // [6]       Ignored
        uint64_t LargePage           : 1;  // [7]       0 = Points to Page Directory, 1 = 1GB Large Page
        uint64_t Ignored2            : 4;  // [8..11]   OS Use
        uint64_t PageFrameNumber     : 40; // [12..51]  PD Physical Base >> 12
        uint64_t Ignored3            : 11; // [52..62]  OS Use
        uint64_t ExecuteDisable      : 1;  // [63]      Execute Disable (NX)
    };
    // 1GB Direct Physical Page Mapping
    struct {
        uint64_t Present1GB          : 1;  // [0]
        uint64_t ReadWrite1GB        : 1;  // [1]
        uint64_t UserSupervisor1GB   : 1;  // [2]
        uint64_t PageWriteThrough1GB : 1;  // [3]
        uint64_t PageCacheDisable1GB : 1;  // [4]
        uint64_t Accessed1GB         : 1;  // [5]
        uint64_t Dirty1GB            : 1;  // [6]       1 = Page modified
        uint64_t LargePage1GB        : 1;  // [7]       Must be 1 for 1GB page
        uint64_t Global1GB           : 1;  // [8]       1 = Global translation (not flushed on CR3 reload)
        uint64_t Ignored1GB          : 3;  // [9..11]
        uint64_t Pat1GB              : 1;  // [12]      Page Attribute Table bit
        uint64_t Reserved1GB         : 17; // [13..29]  Must be 0
        uint64_t PageFrameNumber1GB  : 22; // [30..51]  1GB Physical Base >> 30
        uint64_t Ignored31GB         : 11; // [52..62]
        uint64_t ExecuteDisable1GB   : 1;  // [63]
    };
};

// ============================================================================
// Page Directory Entry (Level 2 Page Directory)
// ============================================================================
union PD_ENTRY_64 {
    uint64_t Value;
    // Standard PDE pointing to Page Table (4KB hierarchy)
    struct {
        uint64_t Present             : 1;  // [0]
        uint64_t ReadWrite           : 1;  // [1]
        uint64_t UserSupervisor      : 1;  // [2]
        uint64_t PageWriteThrough    : 1;  // [3]
        uint64_t PageCacheDisable    : 1;  // [4]
        uint64_t Accessed            : 1;  // [5]
        uint64_t Ignored1            : 1;  // [6]
        uint64_t LargePage           : 1;  // [7]       0 = Points to PT, 1 = 2MB Large Page
        uint64_t Ignored2            : 4;  // [8..11]
        uint64_t PageFrameNumber     : 40; // [12..51]  Page Table Physical Base >> 12
        uint64_t Ignored3            : 11; // [52..62]
        uint64_t ExecuteDisable      : 1;  // [63]
    };
    // 2MB Direct Physical Large Page Mapping
    struct {
        uint64_t Present2MB          : 1;  // [0]
        uint64_t ReadWrite2MB        : 1;  // [1]
        uint64_t UserSupervisor2MB   : 1;  // [2]
        uint64_t PageWriteThrough2MB : 1;  // [3]
        uint64_t PageCacheDisable2MB : 1;  // [4]
        uint64_t Accessed2MB         : 1;  // [5]
        uint64_t Dirty2MB            : 1;  // [6]
        uint64_t LargePage2MB        : 1;  // [7]       Must be 1 for 2MB
        uint64_t Global2MB           : 1;  // [8]
        uint64_t Ignored2MB          : 3;  // [9..11]
        uint64_t Pat2MB              : 1;  // [12]      Page Attribute Table
        uint64_t Reserved2MB         : 8;  // [13..20]  Must be 0
        uint64_t PageFrameNumber2MB  : 31; // [21..51]  2MB Physical Base >> 21
        uint64_t Ignored32MB         : 11; // [52..62]
        uint64_t ExecuteDisable2MB   : 1;  // [63]
    };
};

// ============================================================================
// Page Table Entry (Level 1 Page Table - 4KB Page)
// ============================================================================
union PT_ENTRY_64 {
    uint64_t Value;
    struct {
        uint64_t Present             : 1;  // [0]       1 = Present in RAM
        uint64_t ReadWrite           : 1;  // [1]       0 = Read-Only, 1 = Read/Write
        uint64_t UserSupervisor      : 1;  // [2]       0 = Supervisor (Ring 0), 1 = User (Ring 3)
        uint64_t PageWriteThrough    : 1;  // [3]       1 = Write-through
        uint64_t PageCacheDisable    : 1;  // [4]       1 = Cache disabled
        uint64_t Accessed            : 1;  // [5]       1 = Page accessed
        uint64_t Dirty               : 1;  // [6]       1 = Page written / dirty
        uint64_t Pat                 : 1;  // [7]       Page Attribute Table index bit
        uint64_t Global              : 1;  // [8]       1 = Translation preserved across CR3 reload
        uint64_t Ignored1            : 3;  // [9..11]   OS Use / Prototype PTE flags
        uint64_t PageFrameNumber     : 40; // [12..51]  Physical Page Frame Number (PFN)
        uint64_t Ignored2            : 11; // [52..62]  OS Use / Protection mask
        uint64_t ExecuteDisable      : 1;  // [63]      1 = No Execute (NX / DEP)
    };
};

} // namespace unpd::mmu

#pragma pack(pop)

#endif // UNPD_PAGING_TYPES_HPP
