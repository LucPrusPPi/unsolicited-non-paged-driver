-- ============================================================================
-- UNPD Driver Framework - Pure Lua x86-64 MMU Paging & Translation Simulator
-- ============================================================================

local MmuEngine = {}

-- Architectural Constants
MmuEngine.PAGE_SIZE_4KB = 4096
MmuEngine.PAGE_SIZE_2MB = 2 * 1024 * 1024
MmuEngine.PAGE_SIZE_1GB = 1024 * 1024 * 1024
MmuEngine.CANONICAL_MASK = 0x0000FFFFFFFFFFFF
MmuEngine.SIGN_EXT_MASK  = 0xFFFF000000000000

-- Bitfield Flags
MmuEngine.PTE_PRESENT         = 0x001
MmuEngine.PTE_READ_WRITE      = 0x002
MmuEngine.PTE_USER_SUPERVISOR = 0x004
MmuEngine.PTE_WRITE_THROUGH   = 0x008
MmuEngine.PTE_CACHE_DISABLE   = 0x010
MmuEngine.PTE_ACCESSED        = 0x020
MmuEngine.PTE_DIRTY           = 0x040
MmuEngine.PTE_LARGE_PAGE      = 0x080
MmuEngine.PTE_GLOBAL          = 0x100
MmuEngine.PTE_EXECUTE_DISABLE = 0x8000000000000000

--- Validates whether a 64-bit virtual address is in 48-bit canonical form
function MmuEngine.is_canonical(va)
    if type(va) ~= "number" then return false end
    -- Check bits 47..63 sign extension
    local bit47 = math.floor(va / (2^47)) % 2
    local upper = math.floor(va / (2^48))
    if bit47 == 0 then
        return upper == 0
    else
        return upper == 0xFFFF
    end
end

--- Decomposes a 64-bit virtual address into 4-level MMU page indices
function MmuEngine.decompose_address(va)
    local offset4kb = va % 4096
    local offset2mb = va % (2 * 1024 * 1024)
    local offset1gb = va % (1024 * 1024 * 1024)

    local pt_idx   = math.floor(va / 4096) % 512
    local pd_idx   = math.floor(va / (4096 * 512)) % 512
    local pdpt_idx = math.floor(va / (4096 * 512 * 512)) % 512
    local pml4_idx = math.floor(va / (4096 * 512 * 512 * 512)) % 512

    return {
        VirtualAddress = va,
        IsCanonical    = MmuEngine.is_canonical(va),
        Offset4KB      = offset4kb,
        Offset2MB      = offset2mb,
        Offset1GB      = offset1gb,
        PtIndex        = pt_idx,
        PdIndex        = pd_idx,
        PdptIndex      = pdpt_idx,
        Pml4Index      = pml4_idx
    }
end

--- Helper for aligning addresses
function MmuEngine.align_up(val, alignment)
    alignment = alignment or 4096
    return math.floor((val + alignment - 1) / alignment) * alignment
end

function MmuEngine.align_down(val, alignment)
    alignment = alignment or 4096
    return math.floor(val / alignment) * alignment
end

--- Creates a virtual mock Page Table Hierarchy
function MmuEngine.new_hierarchy(cr3_pfn)
    local self = {
        Cr3Pfn = cr3_pfn or 0x1000,
        Pml4 = {},
        Pdpt = {},
        Pd = {},
        Pt = {},
        TlbCache = {}
    }

    --- Maps a virtual 4KB page to physical PFN
    function self:map_page(va, pfn, flags)
        flags = flags or (MmuEngine.PTE_PRESENT + MmuEngine.PTE_READ_WRITE)
        local dec = MmuEngine.decompose_address(va)

        local pml4_key = dec.Pml4Index
        local pdpt_key = string.format("%d:%d", dec.Pml4Index, dec.PdptIndex)
        local pd_key   = string.format("%d:%d:%d", dec.Pml4Index, dec.PdptIndex, dec.PdIndex)
        local pt_key   = string.format("%d:%d:%d:%d", dec.Pml4Index, dec.PdptIndex, dec.PdIndex, dec.PtIndex)

        self.Pml4[pml4_key] = MmuEngine.PTE_PRESENT + MmuEngine.PTE_READ_WRITE
        self.Pdpt[pdpt_key] = MmuEngine.PTE_PRESENT + MmuEngine.PTE_READ_WRITE
        self.Pd[pd_key]     = MmuEngine.PTE_PRESENT + MmuEngine.PTE_READ_WRITE
        self.Pt[pt_key]     = { Pfn = pfn, Flags = flags }

        -- Invalidate TLB entry for this VA
        local page_va = MmuEngine.align_down(va, 4096)
        self.TlbCache[page_va] = nil
    end

    --- Translates virtual address to physical address via 4-level page walk
    function self:translate(va, is_write, is_user)
        is_write = is_write or false
        is_user = is_user or false

        local page_va = MmuEngine.align_down(va, 4096)
        local cached = self.TlbCache[page_va]
        if cached then
            return {
                Status = "TLB_HIT",
                PhysicalAddress = (cached.Pfn * 4096) + (va % 4096),
                Pfn = cached.Pfn,
                Flags = cached.Flags
            }
        end

        local dec = MmuEngine.decompose_address(va)
        if not dec.IsCanonical then
            return { Status = "PAGE_FAULT_NON_CANONICAL", Code = 0xC0000005 }
        end

        local pml4_key = dec.Pml4Index
        if not self.Pml4[pml4_key] then
            return { Status = "PAGE_FAULT_PML4_NOT_PRESENT", Code = 0xC0000005 }
        end

        local pdpt_key = string.format("%d:%d", dec.Pml4Index, dec.PdptIndex)
        if not self.Pdpt[pdpt_key] then
            return { Status = "PAGE_FAULT_PDPT_NOT_PRESENT", Code = 0xC0000005 }
        end

        local pd_key = string.format("%d:%d:%d", dec.Pml4Index, dec.PdptIndex, dec.PdIndex)
        if not self.Pd[pd_key] then
            return { Status = "PAGE_FAULT_PD_NOT_PRESENT", Code = 0xC0000005 }
        end

        local pt_key = string.format("%d:%d:%d:%d", dec.Pml4Index, dec.PdptIndex, dec.PdIndex, dec.PtIndex)
        local pte = self.Pt[pt_key]
        if not pte or (pte.Flags % 2 == 0) then
            return { Status = "PAGE_FAULT_PTE_NOT_PRESENT", Code = 0xC0000005 }
        end

        -- Cache in TLB
        self.TlbCache[page_va] = { Pfn = pte.Pfn, Flags = pte.Flags }

        local pa = (pte.Pfn * 4096) + dec.Offset4KB
        return {
            Status = "SUCCESS",
            PhysicalAddress = pa,
            Pfn = pte.Pfn,
            Flags = pte.Flags
        }
    end

    --- Flushes TLB cache
    function self:flush_tlb()
        self.TlbCache = {}
    end

    return self
end

--- Self-test suite for MMU simulation
function MmuEngine.self_test()
    print("========================================================")
    print(" UNPD Lua MMU Engine Self-Test")
    print("========================================================")

    local mmu = MmuEngine.new_hierarchy(0x2000)

    -- Test Canonical Address Decomposition
    local va = 0x7FFF12345678
    local dec = MmuEngine.decompose_address(va)
    assert(dec.Offset4KB == 0x678, "Offset4KB calculation failed")
    assert(dec.IsCanonical == true, "Canonical check failed")

    -- Map test page
    mmu:map_page(0x7FFF12340000, 0xABCDE, MmuEngine.PTE_PRESENT + MmuEngine.PTE_READ_WRITE)

    -- Translate
    local res = mmu:translate(0x7FFF12340500, true, false)
    assert(res.Status == "SUCCESS", "Translation failed: " .. tostring(res.Status))
    assert(res.PhysicalAddress == (0xABCDE * 4096 + 0x500), "Physical address calculation mismatch")

    -- Translate unmapped
    local unmapped = mmu:translate(0x7FFF99990000, false, false)
    assert(unmapped.Status ~= "SUCCESS", "Expected page fault on unmapped address")

    print("[+] MMU Hierarchy & Page Walker simulated successfully.")
    return true
end

if not pcall(debug.getlocal, 4, 1) then
    MmuEngine.self_test()
end

return MmuEngine
