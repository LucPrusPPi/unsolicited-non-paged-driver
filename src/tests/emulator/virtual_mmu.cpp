#include "virtual_mmu.hpp"
#include <algorithm>
#include <stdexcept>

namespace unpd::test::emulator {

VirtualMmu::VirtualMmu(size_t ramSize)
    : m_physicalRam(ramSize, 0), m_maxPfn(ramSize / PAGE_SIZE) {
    FlushTlb();
}

uint64_t VirtualMmu::AllocatePhysicalPage() {
    if (m_nextPfn >= m_maxPfn) {
        return 0; // Out of physical RAM
    }
    const uint64_t pfn = m_nextPfn++;
    const uint64_t pa = pfn * PAGE_SIZE;
    std::memset(m_physicalRam.data() + pa, 0, PAGE_SIZE);
    return pa;
}

uint8_t* VirtualMmu::GetPhysicalPointer(uint64_t physicalAddress) {
    if (physicalAddress >= m_physicalRam.size()) {
        return nullptr;
    }
    return m_physicalRam.data() + physicalAddress;
}

const uint8_t* VirtualMmu::GetPhysicalPointer(uint64_t physicalAddress) const {
    if (physicalAddress >= m_physicalRam.size()) {
        return nullptr;
    }
    return m_physicalRam.data() + physicalAddress;
}

bool VirtualMmu::ReadPhysical(uint64_t physicalAddress, void* buffer, size_t size) const {
    if (!buffer || size == 0 || physicalAddress + size > m_physicalRam.size()) {
        return false;
    }
    std::memcpy(buffer, m_physicalRam.data() + physicalAddress, size);
    return true;
}

bool VirtualMmu::WritePhysical(uint64_t physicalAddress, const void* buffer, size_t size) {
    if (!buffer || size == 0 || physicalAddress + size > m_physicalRam.size()) {
        return false;
    }
    std::memcpy(m_physicalRam.data() + physicalAddress, buffer, size);
    return true;
}

uint64_t VirtualMmu::CreatePml4() {
    return AllocatePhysicalPage();
}

bool VirtualMmu::MapPage(uint64_t cr3, uint64_t virtualAddress, uint64_t physicalAddress, uint64_t flags) {
    if (!cr3 || !IsCanonical(virtualAddress)) {
        return false;
    }

    const uint64_t pml4Index = (virtualAddress >> 39) & 0x1FFULL;
    const uint64_t pdptIndex = (virtualAddress >> 30) & 0x1FFULL;
    const uint64_t pdIndex   = (virtualAddress >> 21) & 0x1FFULL;
    const uint64_t ptIndex   = (virtualAddress >> 12) & 0x1FFULL;

    // PML4 Entry
    auto* pml4 = reinterpret_cast<uint64_t*>(GetPhysicalPointer(cr3 & ~0xFFFULL));
    if (!pml4) return false;

    if (!(pml4[pml4Index] & PageFlags::Present)) {
        uint64_t pdptPa = AllocatePhysicalPage();
        if (!pdptPa) return false;
        pml4[pml4Index] = pdptPa | PageFlags::Present | PageFlags::ReadWrite | (flags & PageFlags::UserSupervisor);
    }

    // PDPT Entry
    auto* pdpt = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pml4[pml4Index] & ~0xFFFULL));
    if (!pdpt) return false;

    if (!(pdpt[pdptIndex] & PageFlags::Present)) {
        uint64_t pdPa = AllocatePhysicalPage();
        if (!pdPa) return false;
        pdpt[pdptIndex] = pdPa | PageFlags::Present | PageFlags::ReadWrite | (flags & PageFlags::UserSupervisor);
    }

    // PD Entry
    auto* pd = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pdpt[pdptIndex] & ~0xFFFULL));
    if (!pd) return false;

    if (!(pd[pdIndex] & PageFlags::Present)) {
        uint64_t ptPa = AllocatePhysicalPage();
        if (!ptPa) return false;
        pd[pdIndex] = ptPa | PageFlags::Present | PageFlags::ReadWrite | (flags & PageFlags::UserSupervisor);
    }

    // PT Entry
    auto* pt = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pd[pdIndex] & ~0xFFFULL));
    if (!pt) return false;

    pt[ptIndex] = (physicalAddress & ~0xFFFULL) | flags;
    Invlpg(virtualAddress);
    return true;
}

bool VirtualMmu::MapLargePage2MB(uint64_t cr3, uint64_t virtualAddress, uint64_t physicalAddress2MB, uint64_t flags) {
    if (!cr3 || !IsCanonical(virtualAddress)) return false;

    const uint64_t pml4Index = (virtualAddress >> 39) & 0x1FFULL;
    const uint64_t pdptIndex = (virtualAddress >> 30) & 0x1FFULL;
    const uint64_t pdIndex   = (virtualAddress >> 21) & 0x1FFULL;

    auto* pml4 = reinterpret_cast<uint64_t*>(GetPhysicalPointer(cr3 & ~0xFFFULL));
    if (!pml4) return false;

    if (!(pml4[pml4Index] & PageFlags::Present)) {
        uint64_t pdptPa = AllocatePhysicalPage();
        if (!pdptPa) return false;
        pml4[pml4Index] = pdptPa | PageFlags::Present | PageFlags::ReadWrite | (flags & PageFlags::UserSupervisor);
    }

    auto* pdpt = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pml4[pml4Index] & ~0xFFFULL));
    if (!pdpt) return false;

    if (!(pdpt[pdptIndex] & PageFlags::Present)) {
        uint64_t pdPa = AllocatePhysicalPage();
        if (!pdPa) return false;
        pdpt[pdptIndex] = pdPa | PageFlags::Present | PageFlags::ReadWrite | (flags & PageFlags::UserSupervisor);
    }

    auto* pd = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pdpt[pdptIndex] & ~0xFFFULL));
    if (!pd) return false;

    pd[pdIndex] = (physicalAddress2MB & ~0x1FFFFFULL) | flags | PageFlags::LargePage;
    Invlpg(virtualAddress);
    return true;
}

bool VirtualMmu::MapHugePage1GB(uint64_t cr3, uint64_t virtualAddress, uint64_t physicalAddress1GB, uint64_t flags) {
    if (!cr3 || !IsCanonical(virtualAddress)) return false;

    const uint64_t pml4Index = (virtualAddress >> 39) & 0x1FFULL;
    const uint64_t pdptIndex = (virtualAddress >> 30) & 0x1FFULL;

    auto* pml4 = reinterpret_cast<uint64_t*>(GetPhysicalPointer(cr3 & ~0xFFFULL));
    if (!pml4) return false;

    if (!(pml4[pml4Index] & PageFlags::Present)) {
        uint64_t pdptPa = AllocatePhysicalPage();
        if (!pdptPa) return false;
        pml4[pml4Index] = pdptPa | PageFlags::Present | PageFlags::ReadWrite | (flags & PageFlags::UserSupervisor);
    }

    auto* pdpt = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pml4[pml4Index] & ~0xFFFULL));
    if (!pdpt) return false;

    pdpt[pdptIndex] = (physicalAddress1GB & ~0x3FFFFFFFULL) | flags | PageFlags::LargePage;
    Invlpg(virtualAddress);
    return true;
}

bool VirtualMmu::SetPageFlags(uint64_t cr3, uint64_t virtualAddress, uint64_t flags) {
    if (!cr3 || !IsCanonical(virtualAddress)) return false;

    const uint64_t pml4Index = (virtualAddress >> 39) & 0x1FFULL;
    const uint64_t pdptIndex = (virtualAddress >> 30) & 0x1FFULL;
    const uint64_t pdIndex   = (virtualAddress >> 21) & 0x1FFULL;
    const uint64_t ptIndex   = (virtualAddress >> 12) & 0x1FFULL;

    auto* pml4 = reinterpret_cast<uint64_t*>(GetPhysicalPointer(cr3 & ~0xFFFULL));
    if (!pml4 || !(pml4[pml4Index] & PageFlags::Present)) return false;

    auto* pdpt = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pml4[pml4Index] & ~0xFFFULL));
    if (!pdpt || !(pdpt[pdptIndex] & PageFlags::Present)) return false;

    if (pdpt[pdptIndex] & PageFlags::LargePage) {
        uint64_t pa = pdpt[pdptIndex] & 0x000FFFFFC0000000ULL;
        pdpt[pdptIndex] = pa | flags | PageFlags::LargePage;
        Invlpg(virtualAddress);
        return true;
    }

    auto* pd = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pdpt[pdptIndex] & ~0xFFFULL));
    if (!pd || !(pd[pdIndex] & PageFlags::Present)) return false;

    if (pd[pdIndex] & PageFlags::LargePage) {
        uint64_t pa = pd[pdIndex] & 0x000FFFFFFFE00000ULL;
        pd[pdIndex] = pa | flags | PageFlags::LargePage;
        Invlpg(virtualAddress);
        return true;
    }

    auto* pt = reinterpret_cast<uint64_t*>(GetPhysicalPointer(pd[pdIndex] & ~0xFFFULL));
    if (!pt || !(pt[ptIndex] & PageFlags::Present)) return false;

    uint64_t pa = pt[ptIndex] & 0x000FFFFFFFFFF000ULL;
    pt[ptIndex] = pa | flags;
    Invlpg(virtualAddress);
    return true;
}

unpd::kstd::expected<uint64_t, PageFaultException> VirtualMmu::Translate(
    uint64_t cr3,
    uint64_t virtualAddress,
    bool isWrite,
    bool isUser,
    bool isInstructionFetch
) {
    using ErrorType = unpd::kstd::expected<uint64_t, PageFaultException>;

    if (!IsCanonical(virtualAddress)) {
        PageFaultException ex{};
        ex.FaultingVirtualAddress = virtualAddress;
        ex.ErrorCode.Present = 0;
        ex.ErrorCode.Write = isWrite ? 1 : 0;
        ex.ErrorCode.User = isUser ? 1 : 0;
        ex.FaultLevel = 4;
        return ErrorType::error(ex);
    }

    // Check TLB Cache
    const uint64_t vaPage = virtualAddress & ~0xFFFULL;
    m_tlbClock++;
    for (auto& entry : m_tlb) {
        if (entry.Valid && entry.Cr3 == (cr3 & ~0xFFFULL) && entry.VirtualPage == vaPage) {
            entry.AccessTimestamp = m_tlbClock;
            m_tlbHits++;

            // Check permissions from cached flags
            if (isWrite && !(entry.Flags & PageFlags::ReadWrite) && (isUser || m_cr0_wp)) {
                PageFaultException ex{};
                ex.FaultingVirtualAddress = virtualAddress;
                ex.ErrorCode.Present = 1;
                ex.ErrorCode.Write = 1;
                ex.ErrorCode.User = isUser ? 1 : 0;
                ex.FaultLevel = 1;
                return ErrorType::error(ex);
            }
            if (isUser && !(entry.Flags & PageFlags::UserSupervisor)) {
                PageFaultException ex{};
                ex.FaultingVirtualAddress = virtualAddress;
                ex.ErrorCode.Present = 1;
                ex.ErrorCode.User = 1;
                ex.FaultLevel = 1;
                return ErrorType::error(ex);
            }
            if (isInstructionFetch && m_efer_nxe && (entry.Flags & PageFlags::NoExecute)) {
                PageFaultException ex{};
                ex.FaultingVirtualAddress = virtualAddress;
                ex.ErrorCode.Present = 1;
                ex.ErrorCode.InstructionFetch = 1;
                ex.FaultLevel = 1;
                return ErrorType::error(ex);
            }

            return entry.PhysicalPage + (virtualAddress & 0xFFFULL);
        }
    }
    m_tlbMisses++;

    // PML4 Level (Level 4)
    const uint64_t pml4Index = (virtualAddress >> 39) & 0x1FFULL;
    auto* pml4 = reinterpret_cast<const uint64_t*>(GetPhysicalPointer(cr3 & ~0xFFFULL));
    if (!pml4 || !(pml4[pml4Index] & PageFlags::Present)) {
        PageFaultException ex{};
        ex.FaultingVirtualAddress = virtualAddress;
        ex.ErrorCode.Present = 0;
        ex.ErrorCode.Write = isWrite ? 1 : 0;
        ex.ErrorCode.User = isUser ? 1 : 0;
        ex.FaultLevel = 4;
        return ErrorType::error(ex);
    }
    uint64_t accumFlags = pml4[pml4Index];

    // PDPT Level (Level 3)
    const uint64_t pdptIndex = (virtualAddress >> 30) & 0x1FFULL;
    auto* pdpt = reinterpret_cast<const uint64_t*>(GetPhysicalPointer(pml4[pml4Index] & ~0xFFFULL));
    if (!pdpt || !(pdpt[pdptIndex] & PageFlags::Present)) {
        PageFaultException ex{};
        ex.FaultingVirtualAddress = virtualAddress;
        ex.ErrorCode.Present = 0;
        ex.ErrorCode.Write = isWrite ? 1 : 0;
        ex.ErrorCode.User = isUser ? 1 : 0;
        ex.FaultLevel = 3;
        return ErrorType::error(ex);
    }
    accumFlags &= pdpt[pdptIndex];

    // 1GB Huge Page Check
    if (pdpt[pdptIndex] & PageFlags::LargePage) {
        if (isWrite && !(accumFlags & PageFlags::ReadWrite) && (isUser || m_cr0_wp)) {
            PageFaultException ex{};
            ex.FaultingVirtualAddress = virtualAddress;
            ex.ErrorCode.Present = 1;
            ex.ErrorCode.Write = 1;
            ex.ErrorCode.User = isUser ? 1 : 0;
            ex.FaultLevel = 3;
            return ErrorType::error(ex);
        }
        const uint64_t pa = (pdpt[pdptIndex] & 0x000FFFFFC0000000ULL) + (virtualAddress & 0x3FFFFFFFULL);
        return pa;
    }

    // PD Level (Level 2)
    const uint64_t pdIndex = (virtualAddress >> 21) & 0x1FFULL;
    auto* pd = reinterpret_cast<const uint64_t*>(GetPhysicalPointer(pdpt[pdptIndex] & ~0xFFFULL));
    if (!pd || !(pd[pdIndex] & PageFlags::Present)) {
        PageFaultException ex{};
        ex.FaultingVirtualAddress = virtualAddress;
        ex.ErrorCode.Present = 0;
        ex.ErrorCode.Write = isWrite ? 1 : 0;
        ex.ErrorCode.User = isUser ? 1 : 0;
        ex.FaultLevel = 2;
        return ErrorType::error(ex);
    }
    accumFlags &= pd[pdIndex];

    // 2MB Large Page Check
    if (pd[pdIndex] & PageFlags::LargePage) {
        if (isWrite && !(accumFlags & PageFlags::ReadWrite) && (isUser || m_cr0_wp)) {
            PageFaultException ex{};
            ex.FaultingVirtualAddress = virtualAddress;
            ex.ErrorCode.Present = 1;
            ex.ErrorCode.Write = 1;
            ex.ErrorCode.User = isUser ? 1 : 0;
            ex.FaultLevel = 2;
            return ErrorType::error(ex);
        }
        const uint64_t pa = (pd[pdIndex] & 0x000FFFFFFFE00000ULL) + (virtualAddress & 0x1FFFFFULL);
        return pa;
    }

    // PT Level (Level 1)
    const uint64_t ptIndex = (virtualAddress >> 12) & 0x1FFULL;
    auto* pt = reinterpret_cast<const uint64_t*>(GetPhysicalPointer(pd[pdIndex] & ~0xFFFULL));
    if (!pt || !(pt[ptIndex] & PageFlags::Present)) {
        PageFaultException ex{};
        ex.FaultingVirtualAddress = virtualAddress;
        ex.ErrorCode.Present = 0;
        ex.ErrorCode.Write = isWrite ? 1 : 0;
        ex.ErrorCode.User = isUser ? 1 : 0;
        ex.FaultLevel = 1;
        return ErrorType::error(ex);
    }
    accumFlags &= pt[ptIndex];

    // Permission checks
    if (isWrite && !(accumFlags & PageFlags::ReadWrite) && (isUser || m_cr0_wp)) {
        PageFaultException ex{};
        ex.FaultingVirtualAddress = virtualAddress;
        ex.ErrorCode.Present = 1;
        ex.ErrorCode.Write = 1;
        ex.ErrorCode.User = isUser ? 1 : 0;
        ex.FaultLevel = 1;
        return ErrorType::error(ex);
    }

    if (isUser && !(accumFlags & PageFlags::UserSupervisor)) {
        PageFaultException ex{};
        ex.FaultingVirtualAddress = virtualAddress;
        ex.ErrorCode.Present = 1;
        ex.ErrorCode.User = 1;
        ex.FaultLevel = 1;
        return ErrorType::error(ex);
    }

    if (isInstructionFetch && m_efer_nxe && (pt[ptIndex] & PageFlags::NoExecute)) {
        PageFaultException ex{};
        ex.FaultingVirtualAddress = virtualAddress;
        ex.ErrorCode.Present = 1;
        ex.ErrorCode.InstructionFetch = 1;
        ex.FaultLevel = 1;
        return ErrorType::error(ex);
    }

    const uint64_t physicalPage = pt[ptIndex] & 0x000FFFFFFFFFF000ULL;
    const uint64_t finalPa = physicalPage + (virtualAddress & 0xFFFULL);

    // Insert into TLB via LRU
    size_t lruIndex = 0;
    uint64_t oldest = UINT64_MAX;
    for (size_t i = 0; i < TLB_CAPACITY; ++i) {
        if (!m_tlb[i].Valid) {
            lruIndex = i;
            break;
        }
        if (m_tlb[i].AccessTimestamp < oldest) {
            oldest = m_tlb[i].AccessTimestamp;
            lruIndex = i;
        }
    }

    m_tlb[lruIndex].Valid = true;
    m_tlb[lruIndex].Cr3 = cr3 & ~0xFFFULL;
    m_tlb[lruIndex].VirtualPage = vaPage;
    m_tlb[lruIndex].PhysicalPage = physicalPage;
    m_tlb[lruIndex].Flags = pt[ptIndex];
    m_tlb[lruIndex].AccessTimestamp = m_tlbClock;

    return finalPa;
}

bool VirtualMmu::ReadVirtual(uint64_t cr3, uint64_t virtualAddress, void* buffer, size_t size, size_t* bytesRead) {
    if (!cr3 || !buffer || size == 0) {
        if (bytesRead) *bytesRead = 0;
        return false;
    }

    size_t total = 0;
    auto* dest = static_cast<uint8_t*>(buffer);

    while (total < size) {
        const uint64_t currentVa = virtualAddress + total;
        auto result = Translate(cr3, currentVa, false, false, false);
        if (!result.has_value()) {
            if (bytesRead) *bytesRead = total;
            return false;
        }

        const size_t pageOffset = currentVa & 0xFFFULL;
        const size_t chunk = std::min(size - total, PAGE_SIZE - pageOffset);

        if (!ReadPhysical(result.value(), dest + total, chunk)) {
            if (bytesRead) *bytesRead = total;
            return false;
        }
        total += chunk;
    }

    if (bytesRead) *bytesRead = total;
    return true;
}

bool VirtualMmu::WriteVirtual(uint64_t cr3, uint64_t virtualAddress, const void* buffer, size_t size, size_t* bytesWritten) {
    if (!cr3 || !buffer || size == 0) {
        if (bytesWritten) *bytesWritten = 0;
        return false;
    }

    size_t total = 0;
    const auto* src = static_cast<const uint8_t*>(buffer);

    while (total < size) {
        const uint64_t currentVa = virtualAddress + total;
        auto result = Translate(cr3, currentVa, true, false, false);
        if (!result.has_value()) {
            if (bytesWritten) *bytesWritten = total;
            return false;
        }

        const size_t pageOffset = currentVa & 0xFFFULL;
        const size_t chunk = std::min(size - total, PAGE_SIZE - pageOffset);

        if (!WritePhysical(result.value(), src + total, chunk)) {
            if (bytesWritten) *bytesWritten = total;
            return false;
        }
        total += chunk;
    }

    if (bytesWritten) *bytesWritten = total;
    return true;
}

void VirtualMmu::Invlpg(uint64_t virtualAddress) {
    const uint64_t vaPage = virtualAddress & ~0xFFFULL;
    for (auto& entry : m_tlb) {
        if (entry.Valid && entry.VirtualPage == vaPage) {
            entry.Valid = false;
        }
    }
}

void VirtualMmu::FlushTlb() {
    for (auto& entry : m_tlb) {
        entry.Valid = false;
    }
}

} // namespace unpd::test::emulator
