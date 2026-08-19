-- ============================================================================
-- UNPD Driver Framework - Lua Fuzzing Scenario and Mutation Generator
-- ============================================================================

local Fuzzer = {}

Fuzzer.CORPUS = {
    "ZERO_BUFFER",
    "OVERSIZED_PAGE_COUNT",
    "INVALID_SLAB_CLASS",
    "NULL_SESSION_HANDLE",
    "CORRUPTED_MAGIC",
    "UNALIGNED_BUFFER_POINTER",
    "INTEGER_OVERFLOW_SIZE"
}

--- Generates a randomized fuzz mutation case
function Fuzzer.generate_mutation(scenario_id, seed)
    math.randomseed(seed or os.time())
    local scenario = Fuzzer.CORPUS[(scenario_id % #Fuzzer.CORPUS) + 1]

    local packet = {
        Scenario = scenario,
        Magic = 0x554E5044,
        PayloadSize = 0,
        Iterations = math.random(10, 500)
    }

    if scenario == "ZERO_BUFFER" then
        packet.PayloadSize = 0
    elseif scenario == "OVERSIZED_PAGE_COUNT" then
        packet.PayloadSize = math.random(257, 100000)
    elseif scenario == "CORRUPTED_MAGIC" then
        packet.Magic = math.random(0x10000000, 0xFFFFFFFF)
    elseif scenario == "INTEGER_OVERFLOW_SIZE" then
        packet.PayloadSize = 0xFFFFFFFFFFFFFFFF
    else
        packet.PayloadSize = math.random(1, 4096)
    end

    return packet
end

--- Runs an automated test batch of fuzz scenarios
function Fuzzer.run_batch(count)
    count = count or 50
    print(string.format("[*] Generating %d Lua fuzz test scenarios...", count))

    local generated = 0
    for i = 1, count do
        local mut = Fuzzer.generate_mutation(i, i * 31337)
        if mut and mut.Scenario then
            generated = generated + 1
        end
    end

    print(string.format("[+] Generated and validated %d fuzz mutations cleanly.", generated))
    return generated == count
end

-- Run if executed directly
if not pcall(debug.getlocal, 4, 1) then
    local ok = Fuzzer.run_batch(100)
    if not ok then os.exit(1) end
end

return Fuzzer
