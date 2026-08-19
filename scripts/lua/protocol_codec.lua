-- ============================================================================
-- UNPD Driver Framework - Pure Lua IOCTL Binary Protocol Codec & Dissector
-- ============================================================================

local Codec = {}

Codec.MAGIC_REQUEST  = 0x554E5044 -- 'UNPD'
Codec.MAGIC_RESPONSE = 0x44504E55 -- 'DPNU'

Codec.IOCTL = {
    PING                   = 0x00222000,
    ALLOCATE_NONPAGED      = 0x00222004,
    FREE_NONPAGED          = 0x00222008,
    QUERY_STATS            = 0x0022200C,
    PROCESS_BUFFER_DIRECT  = 0x00222013,
    PROCESS_BUFFER_NEITHER = 0x00222017,
    MAP_SHARED_MEMORY      = 0x00222018,
    UNMAP_SHARED_MEMORY    = 0x0022201C,
    SWAP_BUFFERS           = 0x00222020,
    SLAB_ALLOC             = 0x00222024,
    SLAB_FREE              = 0x00222028
}

--- Software CRC32 implementation in Lua
function Codec.crc32(data, seed)
    local crc = seed or 0xFFFFFFFF
    for i = 1, #data do
        local byte = string.byte(data, i)
        crc = bit32 and bit32.bxor(crc, byte) or (crc ~ byte)
        for _ = 1, 8 do
            local mask = -(crc % 2)
            if bit32 then
                crc = bit32.bxor(bit32.rshift(crc, 1), bit32.band(0xEDB88320, mask))
            else
                crc = (crc >> 1) ~ (0xEDB88320 & mask)
            end
        end
    end
    return bit32 and bit32.bxor(crc, 0xFFFFFFFF) or (crc ~ 0xFFFFFFFF)
end

--- Serializes a 32-bit integer to 4 little-endian bytes
function Codec.pack_u32(val)
    local b1 = val % 256
    local b2 = math.floor(val / 256) % 256
    local b3 = math.floor(val / 65536) % 256
    local b4 = math.floor(val / 16777216) % 256
    return string.char(b1, b2, b3, b4)
end

--- Deserializes a 32-bit little-endian integer from bytes
function Codec.unpack_u32(str, offset)
    offset = offset or 1
    local b1, b2, b3, b4 = string.byte(str, offset, offset + 3)
    return b1 + (b2 * 256) + (b3 * 65536) + (b4 * 16777216)
end

--- Serializes a 64-bit integer to 8 little-endian bytes
function Codec.pack_u64(val)
    local low = val % 4294967296
    local high = math.floor(val / 4294967296)
    return Codec.pack_u32(low) .. Codec.pack_u32(high)
end

--- Deserializes a 64-bit little-endian integer
function Codec.unpack_u64(str, offset)
    offset = offset or 1
    local low = Codec.unpack_u32(str, offset)
    local high = Codec.unpack_u32(str, offset + 4)
    return low + (high * 4294967296)
end

--- Encode Ping Request Packet
function Codec.encode_ping_request(sequence)
    local magic = Codec.pack_u32(Codec.MAGIC_REQUEST)
    local seq = Codec.pack_u32(sequence or 1)
    local ts = Codec.pack_u64(os.time())
    return magic .. seq .. ts
end

--- Decode Ping Response Packet
function Codec.decode_ping_response(packet_bytes)
    if #packet_bytes < 16 then
        return nil, "Packet too short"
    end
    local magic = Codec.unpack_u32(packet_bytes, 1)
    local seq = Codec.unpack_u32(packet_bytes, 5)
    local ts = Codec.unpack_u64(packet_bytes, 9)

    return {
        Magic = magic,
        SequenceOut = seq,
        KernelTimestamp = ts,
        IsValid = (magic == Codec.MAGIC_RESPONSE)
    }
end

--- Encode Shared Memory Map Request
function Codec.encode_map_request(page_count, slab_class)
    local magic = Codec.pack_u32(Codec.MAGIC_REQUEST)
    local pages = Codec.pack_u32(page_count or 16)
    local class = Codec.pack_u32(slab_class or 0)
    return magic .. pages .. class
end

--- Decode Shared Memory Map Response
function Codec.decode_map_response(packet_bytes)
    if #packet_bytes < 24 then
        return nil, "Packet too short"
    end
    local magic = Codec.unpack_u32(packet_bytes, 1)
    local handle = Codec.unpack_u64(packet_bytes, 5)
    local user_va = Codec.unpack_u64(packet_bytes, 13)
    local mapped_size = Codec.unpack_u32(packet_bytes, 21)

    return {
        Magic = magic,
        SessionHandle = handle,
        UserVirtualAddress = user_va,
        MappedSizeBytes = mapped_size,
        IsValid = (magic == Codec.MAGIC_RESPONSE)
    }
end

--- Dissects any raw packet buffer and prints human-readable inspection
function Codec.dissect_packet(packet_bytes)
    print("--------------------------------------------------------")
    print(string.format(" Packet Dissection (%d bytes):", #packet_bytes))
    local hex = {}
    for i = 1, #packet_bytes do
        table.insert(hex, string.format("%02X", string.byte(packet_bytes, i)))
    end
    print(" Hex: " .. table.concat(hex, " "))

    if #packet_bytes >= 4 then
        local magic = Codec.unpack_u32(packet_bytes, 1)
        if magic == Codec.MAGIC_REQUEST then
            print(" Type: UNPD Client Request (Magic: 0x554E5044)")
        elseif magic == Codec.MAGIC_RESPONSE then
            print(" Type: UNPD Kernel Response (Magic: 0x44504E55)")
        else
            print(string.format(" Type: Unknown / Malformed (Magic: 0x%08X)", magic))
        end
    end
    print("--------------------------------------------------------")
end

--- Self-test suite
function Codec.self_test()
    print("========================================================")
    print(" UNPD Lua Protocol Codec Self-Test")
    print("========================================================")

    -- Ping packet roundtrip
    local req = Codec.encode_ping_request(42)
    assert(#req == 16, "Ping request size mismatch")

    -- Synthetic response
    local resp_bytes = Codec.pack_u32(Codec.MAGIC_RESPONSE) .. Codec.pack_u32(43) .. Codec.pack_u64(1234567890)
    local decoded, err = Codec.decode_ping_response(resp_bytes)
    assert(decoded and decoded.IsValid, err or "Decode failed")
    assert(decoded.SequenceOut == 43, "Sequence mismatch")

    -- Dissect
    Codec.dissect_packet(resp_bytes)

    print("[+] Protocol Codec & Packet Dissector validated.")
    return true
end

if not pcall(debug.getlocal, 4, 1) then
    Codec.self_test()
end

return Codec
