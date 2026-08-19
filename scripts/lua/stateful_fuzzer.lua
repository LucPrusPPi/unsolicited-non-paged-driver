-- ============================================================================
-- UNPD Driver Framework - Pure Lua Stateful Kernel Fuzzer & Mutation Engine
-- ============================================================================

local StatefulFuzzer = {}

StatefulFuzzer.STATES = {
    UNINITIALIZED = 0,
    CONNECTED     = 1,
    MAPPED_MDL    = 2,
    ALLOCATED_POOL= 3,
    CLOSED        = 4
}

StatefulFuzzer.ACTIONS = {
    "PING",
    "ALLOC_POOL",
    "FREE_POOL",
    "MAP_MDL",
    "UNMAP_MDL",
    "SWAP_BUFFERS",
    "SLAB_ALLOC",
    "SLAB_FREE",
    "CORRUPT_HANDLE",
    "OVERSIZED_PAYLOAD"
}

--- Generates a stateful sequence of IOCTL workflow actions
function StatefulFuzzer.generate_workflow(chain_length, seed)
    math.randomseed(seed or os.time())
    chain_length = chain_length or 20

    local workflow = {}
    local current_state = StatefulFuzzer.STATES.UNINITIALIZED
    local active_handles = {}
    local active_sessions = {}

    for step = 1, chain_length do
        local action = StatefulFuzzer.ACTIONS[math.random(1, #StatefulFuzzer.ACTIONS)]
        local item = {
            Step = step,
            Action = action,
            PreviousState = current_state,
            Parameters = {}
        }

        if action == "PING" then
            item.Parameters.Sequence = math.random(1, 100000)
        elseif action == "ALLOC_POOL" then
            item.Parameters.SizeBytes = math.random(1, 65536)
            current_state = StatefulFuzzer.STATES.ALLOCATED_POOL
        elseif action == "MAP_MDL" then
            item.Parameters.PageCount = math.random(1, 64)
            item.Parameters.Class = math.random(0, 3)
            current_state = StatefulFuzzer.STATES.MAPPED_MDL
        elseif action == "SWAP_BUFFERS" then
            item.Parameters.SessionHandle = #active_sessions > 0 and active_sessions[1] or 0xDEADBEEF
        elseif action == "CORRUPT_HANDLE" then
            item.Parameters.InvalidHandle = math.random(0x10000000, 0xFFFFFFFF)
        elseif action == "OVERSIZED_PAYLOAD" then
            item.Parameters.SizeBytes = 0xFFFFFFFFFFFFFFFF
        end

        table.insert(workflow, item)
    end

    return workflow
end

--- Executes a simulated batch of stateful mutations
function StatefulFuzzer.run_simulation(workflow_count, chain_length)
    workflow_count = workflow_count or 10
    chain_length = chain_length or 25
    print("========================================================")
    print(" UNPD Lua Stateful Fuzzing Engine")
    print(string.format(" Workflows: %d | Steps per workflow: %d", workflow_count, chain_length))
    print("========================================================")

    local total_steps = 0
    for w = 1, workflow_count do
        local wf = StatefulFuzzer.generate_workflow(chain_length, w * 42)
        total_steps = total_steps + #wf
    end

    print(string.format("[+] Generated and simulated %d stateful transition steps.", total_steps))
    return true
end

if not pcall(debug.getlocal, 4, 1) then
    StatefulFuzzer.run_simulation(10, 30)
end

return StatefulFuzzer
