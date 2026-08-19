-- ============================================================================
-- UNPD Driver Framework - Lua Test and IOCTL Protocol Automation Engine
-- ============================================================================

local UNPD = {}

-- Magic Markers
UNPD.MAGIC_REQUEST  = 0x554E5044 -- 'UNPD'
UNPD.MAGIC_RESPONSE = 0x44504E55 -- 'DPNU'

-- IOCTL Codes
UNPD.IOCTL = {
    PING                   = 0x00222000, -- 0x800 METHOD_BUFFERED
    ALLOCATE_NONPAGED      = 0x00222004, -- 0x801 METHOD_BUFFERED
    FREE_NONPAGED          = 0x00222008, -- 0x802 METHOD_BUFFERED
    QUERY_STATS            = 0x0022200C, -- 0x803 METHOD_BUFFERED
    PROCESS_BUFFER_DIRECT  = 0x00222013, -- 0x804 METHOD_IN_DIRECT
    PROCESS_BUFFER_NEITHER = 0x00222017, -- 0x805 METHOD_NEITHER
    MAP_SHARED_MEMORY      = 0x00222018, -- 0x806 METHOD_BUFFERED
    UNMAP_SHARED_MEMORY    = 0x0022201C, -- 0x807 METHOD_BUFFERED
    SWAP_BUFFERS           = 0x00222020, -- 0x808 METHOD_BUFFERED
    SLAB_ALLOC             = 0x00222024, -- 0x809 METHOD_BUFFERED
    SLAB_FREE              = 0x00222028  -- 0x80A METHOD_BUFFERED
}

-- Status Codes
UNPD.STATUS = {
    SUCCESS             = 0x00000000,
    UNSUCCESSFUL        = 0xC0000001,
    INVALID_PARAMETER   = 0xC000000D,
    INSUFFICIENT_RES    = 0xC000009A,
    NOT_FOUND           = 0xC0000225,
    TOO_MANY_SESSIONS   = 0xC0000228
}

-- Slab Classes
UNPD.SLAB_CLASS = {
    CLASS_64B  = 0,
    CLASS_256B = 1,
    CLASS_1KB  = 2,
    CLASS_4KB  = 3
}

--- Creates a binary ping request packet
function UNPD.pack_ping_request(sequence, timestamp)
    sequence = sequence or 1
    timestamp = timestamp or os.time()
    return string.format("PING:seq=%d,ts=%d,magic=0x%08X", sequence, timestamp, UNPD.MAGIC_REQUEST)
end

--- Simulates validation of a response packet
function UNPD.validate_ping_response(seq_in, seq_out, magic_resp)
    if magic_resp ~= UNPD.MAGIC_RESPONSE then
        return false, "Invalid response magic: " .. string.format("0x%08X", magic_resp)
    end
    if seq_out ~= (seq_in + 1) then
        return false, string.format("Sequence mismatch: expected %d, got %d", seq_in + 1, seq_out)
    end
    return true, "OK"
end

--- Test runner routine
function UNPD.run_suite()
    print("========================================================")
    print(" UNPD Lua Automation Test Suite")
    print("========================================================")

    local passed = 0
    local total = 0

    local function check(name, condition, msg)
        total = total + 1
        if condition then
            passed = passed + 1
            print(string.format("  [PASS] %s", name))
        else
            print(string.format("  [FAIL] %s: %s", name, msg or "Assertion failed"))
        end
    end

    -- Test 1: Magic Constants
    check("MagicMarkers_Integrity",
        UNPD.MAGIC_REQUEST == 0x554E5044 and UNPD.MAGIC_RESPONSE == 0x44504E55)

    -- Test 2: Ping Validation
    local ok, err = UNPD.validate_ping_response(100, 101, UNPD.MAGIC_RESPONSE)
    check("PingSequence_Increment", ok, err)

    -- Test 3: Slab Classes
    check("SlabClasses_Range",
        UNPD.SLAB_CLASS.CLASS_64B == 0 and UNPD.SLAB_CLASS.CLASS_4KB == 3)

    -- Test 4: IOCTL Table
    local ioctl_count = 0
    for _ in pairs(UNPD.IOCTL) do ioctl_count = ioctl_count + 1 end
    check("IoctlCodes_Count_11", ioctl_count == 11, "Expected 11 IOCTLs")

    print("--------------------------------------------------------")
    print(string.format(" Summary: %d / %d tests passed.", passed, total))
    print("========================================================")

    return passed == total
end

-- Run if executed directly
if not pcall(debug.getlocal, 4, 1) then
    local success = UNPD.run_suite()
    if not success then os.exit(1) end
end

return UNPD
