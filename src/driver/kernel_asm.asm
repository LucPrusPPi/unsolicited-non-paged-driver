;==============================================================================
; UNPD Kernel Assembly Subsystem (x64 MASM)
; Hardware serialization, high-resolution cycle timestamping, fast string streaming,
; ring-0 control and debug registers, descriptor tables, atomic bitwise primitives,
; and hardware-accelerated SSE4.2 CRC32 memory hashing.
;==============================================================================

.code

;------------------------------------------------------------------------------
; uint64_t UnpdReadTsc(void)
; Returns the current value of the processor's time-stamp counter.
;------------------------------------------------------------------------------
UnpdReadTsc PROC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    ret
UnpdReadTsc ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadTscp(uint32_t* aux)
; Reads TSC and IA32_TSC_AUX into *aux with serialization.
;------------------------------------------------------------------------------
UnpdReadTscp PROC
    push    rdi
    mov     rdi, rcx
    rdtscp
    test    rdi, rdi
    jz      @skip_aux
    mov     dword ptr [rdi], ecx
@skip_aux:
    shl     rdx, 32
    or      rax, rdx
    pop     rdi
    ret
UnpdReadTscp ENDP

;------------------------------------------------------------------------------
; void UnpdMemoryFence(void)
; Full hardware load/store serialization fence.
;------------------------------------------------------------------------------
UnpdMemoryFence PROC
    mfence
    ret
UnpdMemoryFence ENDP

;------------------------------------------------------------------------------
; void UnpdLoadFence(void)
; Hardware load serialization fence.
;------------------------------------------------------------------------------
UnpdLoadFence PROC
    lfence
    ret
UnpdLoadFence ENDP

;------------------------------------------------------------------------------
; void UnpdStoreFence(void)
; Hardware store serialization fence.
;------------------------------------------------------------------------------
UnpdStoreFence PROC
    sfence
    ret
UnpdStoreFence ENDP

;------------------------------------------------------------------------------
; void UnpdFastSwapBarrier(void* activePtr, void* standbyPtr)
; Atomic exchange barrier with hardware memory serialization.
;------------------------------------------------------------------------------
UnpdFastSwapBarrier PROC
    mfence
    mov     rax, [rdx]
    xchg    [rcx], rax
    mov     [rdx], rax
    mfence
    ret
UnpdFastSwapBarrier ENDP

;------------------------------------------------------------------------------
; void UnpdFastCopy64(void* destination, const void* source, uint64_t qwordCount)
; Fast QWORD memory block copy using REP MOVSQ.
;------------------------------------------------------------------------------
UnpdFastCopy64 PROC
    push    rsi
    push    rdi
    mov     rdi, rcx
    mov     rsi, rdx
    mov     rcx, r8
    rep     movsq
    pop     rdi
    pop     rsi
    ret
UnpdFastCopy64 ENDP

;------------------------------------------------------------------------------
; void UnpdFastZero64(void* destination, uint64_t qwordCount)
; Fast QWORD zeroing using REP STOSQ.
;------------------------------------------------------------------------------
UnpdFastZero64 PROC
    push    rdi
    mov     rdi, rcx
    mov     rcx, rdx
    xor     rax, rax
    rep     stosq
    pop     rdi
    ret
UnpdFastZero64 ENDP

;------------------------------------------------------------------------------
; void UnpdCpuId(int32_t cpuInfo[4], int32_t functionId)
; Queries CPU identification and feature leaf information.
;------------------------------------------------------------------------------
UnpdCpuId PROC
    push    rbx
    push    rdi
    mov     rdi, rcx
    mov     eax, edx
    xor     ecx, ecx
    cpuid
    mov     dword ptr [rdi], eax
    mov     dword ptr [rdi+4], ebx
    mov     dword ptr [rdi+8], ecx
    mov     dword ptr [rdi+12], edx
    pop     rdi
    pop     rbx
    ret
UnpdCpuId ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadCr0(void)
;------------------------------------------------------------------------------
UnpdReadCr0 PROC
    mov     rax, cr0
    ret
UnpdReadCr0 ENDP

;------------------------------------------------------------------------------
; void UnpdWriteCr0(uint64_t value)
;------------------------------------------------------------------------------
UnpdWriteCr0 PROC
    mov     cr0, rcx
    ret
UnpdWriteCr0 ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadCr2(void)
;------------------------------------------------------------------------------
UnpdReadCr2 PROC
    mov     rax, cr2
    ret
UnpdReadCr2 ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadCr3(void)
;------------------------------------------------------------------------------
UnpdReadCr3 PROC
    mov     rax, cr3
    ret
UnpdReadCr3 ENDP

;------------------------------------------------------------------------------
; void UnpdWriteCr3(uint64_t value)
;------------------------------------------------------------------------------
UnpdWriteCr3 PROC
    mov     cr3, rcx
    ret
UnpdWriteCr3 ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadCr4(void)
;------------------------------------------------------------------------------
UnpdReadCr4 PROC
    mov     rax, cr4
    ret
UnpdReadCr4 ENDP

;------------------------------------------------------------------------------
; void UnpdWriteCr4(uint64_t value)
;------------------------------------------------------------------------------
UnpdWriteCr4 PROC
    mov     cr4, rcx
    ret
UnpdWriteCr4 ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadCr8(void)
; Reads the CR8 Task Priority Register (TPR).
;------------------------------------------------------------------------------
UnpdReadCr8 PROC
    mov     rax, cr8
    ret
UnpdReadCr8 ENDP

;------------------------------------------------------------------------------
; void UnpdWriteCr8(uint64_t value)
; Sets the CR8 Task Priority Register (TPR).
;------------------------------------------------------------------------------
UnpdWriteCr8 PROC
    mov     cr8, rcx
    ret
UnpdWriteCr8 ENDP

;------------------------------------------------------------------------------
; void UnpdInvlpg(const void* virtualAddress)
;------------------------------------------------------------------------------
UnpdInvlpg PROC
    invlpg  byte ptr [rcx]
    ret
UnpdInvlpg ENDP

;------------------------------------------------------------------------------
; void UnpdWbinvd(void)
;------------------------------------------------------------------------------
UnpdWbinvd PROC
    wbinvd
    ret
UnpdWbinvd ENDP

;------------------------------------------------------------------------------
; void UnpdFlushTlb(void)
;------------------------------------------------------------------------------
UnpdFlushTlb PROC
    mov     rax, cr3
    mov     cr3, rax
    ret
UnpdFlushTlb ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadMsr(uint32_t msr)
;------------------------------------------------------------------------------
UnpdReadMsr PROC
    rdmsr
    shl     rdx, 32
    or      rax, rdx
    ret
UnpdReadMsr ENDP

;------------------------------------------------------------------------------
; void UnpdWriteMsr(uint32_t msr, uint64_t value)
;------------------------------------------------------------------------------
UnpdWriteMsr PROC
    mov     rax, rdx
    shr     rdx, 32
    wrmsr
    ret
UnpdWriteMsr ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdGetRflags(void)
;------------------------------------------------------------------------------
UnpdGetRflags PROC
    pushfq
    pop     rax
    ret
UnpdGetRflags ENDP

;------------------------------------------------------------------------------
; void UnpdSetRflags(uint64_t flags)
;------------------------------------------------------------------------------
UnpdSetRflags PROC
    push    rcx
    popfq
    ret
UnpdSetRflags ENDP

;------------------------------------------------------------------------------
; Descriptor Table Registers (GDTR, IDTR, TR, LDTR)
;------------------------------------------------------------------------------
UnpdGetGdt PROC
    sgdt    fword ptr [rcx]
    ret
UnpdGetGdt ENDP

UnpdGetIdt PROC
    sidt    fword ptr [rcx]
    ret
UnpdGetIdt ENDP

UnpdGetTr PROC
    str     ax
    movzx   eax, ax
    ret
UnpdGetTr ENDP

UnpdGetLdtr PROC
    sldt    ax
    movzx   eax, ax
    ret
UnpdGetLdtr ENDP

;------------------------------------------------------------------------------
; Segment Selectors (CS, DS, ES, SS, FS, GS)
;------------------------------------------------------------------------------
UnpdGetCs PROC
    mov     ax, cs
    movzx   eax, ax
    ret
UnpdGetCs ENDP

UnpdGetDs PROC
    mov     ax, ds
    movzx   eax, ax
    ret
UnpdGetDs ENDP

UnpdGetEs PROC
    mov     ax, es
    movzx   eax, ax
    ret
UnpdGetEs ENDP

UnpdGetSs PROC
    mov     ax, ss
    movzx   eax, ax
    ret
UnpdGetSs ENDP

UnpdGetFs PROC
    mov     ax, fs
    movzx   eax, ax
    ret
UnpdGetFs ENDP

UnpdGetGs PROC
    mov     ax, gs
    movzx   eax, ax
    ret
UnpdGetGs ENDP

;------------------------------------------------------------------------------
; Hardware Debug Registers (DR0, DR1, DR2, DR3, DR6, DR7)
;------------------------------------------------------------------------------
UnpdReadDr0 PROC
    mov     rax, dr0
    ret
UnpdReadDr0 ENDP

UnpdWriteDr0 PROC
    mov     dr0, rcx
    ret
UnpdWriteDr0 ENDP

UnpdReadDr1 PROC
    mov     rax, dr1
    ret
UnpdReadDr1 ENDP

UnpdWriteDr1 PROC
    mov     dr1, rcx
    ret
UnpdWriteDr1 ENDP

UnpdReadDr2 PROC
    mov     rax, dr2
    ret
UnpdReadDr2 ENDP

UnpdWriteDr2 PROC
    mov     dr2, rcx
    ret
UnpdWriteDr2 ENDP

UnpdReadDr3 PROC
    mov     rax, dr3
    ret
UnpdReadDr3 ENDP

UnpdWriteDr3 PROC
    mov     dr3, rcx
    ret
UnpdWriteDr3 ENDP

UnpdReadDr6 PROC
    mov     rax, dr6
    ret
UnpdReadDr6 ENDP

UnpdWriteDr6 PROC
    mov     dr6, rcx
    ret
UnpdWriteDr6 ENDP

UnpdReadDr7 PROC
    mov     rax, dr7
    ret
UnpdReadDr7 ENDP

UnpdWriteDr7 PROC
    mov     dr7, rcx
    ret
UnpdWriteDr7 ENDP

;------------------------------------------------------------------------------
; Extended Control Registers (XCR0) & Performance Counters (PMC)
;------------------------------------------------------------------------------
UnpdGetXcr0 PROC
    xor     ecx, ecx
    xgetbv
    shl     rdx, 32
    or      rax, rdx
    ret
UnpdGetXcr0 ENDP

UnpdSetXcr0 PROC
    mov     rax, rcx
    mov     rdx, rcx
    shr     rdx, 32
    xor     ecx, ecx
    xsetbv
    ret
UnpdSetXcr0 ENDP

UnpdReadPmc PROC
    mov     ecx, ecx
    rdpmc
    shl     rdx, 32
    or      rax, rdx
    ret
UnpdReadPmc ENDP

;------------------------------------------------------------------------------
; Atomic Bitwise Primitives (BitSet, BitReset, BitTest)
;------------------------------------------------------------------------------
UnpdAtomicBitSet PROC
    ; RCX = pointer, RDX = bitIndex
    lock bts qword ptr [rcx], rdx
    setc    al
    movzx   eax, al
    ret
UnpdAtomicBitSet ENDP

UnpdAtomicBitReset PROC
    ; RCX = pointer, RDX = bitIndex
    lock btr qword ptr [rcx], rdx
    setc    al
    movzx   eax, al
    ret
UnpdAtomicBitReset ENDP

UnpdAtomicBitTest PROC
    ; RCX = pointer, RDX = bitIndex
    bt      qword ptr [rcx], rdx
    setc    al
    movzx   eax, al
    ret
UnpdAtomicBitTest ENDP

;------------------------------------------------------------------------------
; Hardware-Accelerated SSE4.2 CRC32 Primitives
;------------------------------------------------------------------------------
UnpdComputeCrc32_u8 PROC
    ; ECX = initialCrc, DL = data
    mov     eax, ecx
    crc32   eax, dl
    ret
UnpdComputeCrc32_u8 ENDP

UnpdComputeCrc32_u32 PROC
    ; ECX = initialCrc, EDX = data
    mov     eax, ecx
    crc32   eax, edx
    ret
UnpdComputeCrc32_u32 ENDP

UnpdComputeCrc32_u64 PROC
    ; RCX = initialCrc, RDX = data
    mov     rax, rcx
    crc32   rax, rdx
    ret
UnpdComputeCrc32_u64 ENDP

;------------------------------------------------------------------------------
; uint32_t UnpdComputeCrc32_Buffer(uint32_t initialCrc, const void* buffer, uint64_t length)
; High-throughput QWORD-streaming hardware CRC32 memory hasher.
;------------------------------------------------------------------------------
UnpdComputeCrc32_Buffer PROC
    ; ECX = initialCrc, RDX = buffer, R8 = length
    mov     eax, ecx
    test    rdx, rdx
    jz      @crc_done
    test    r8, r8
    jz      @crc_done

    ; Process 8-byte QWORD chunks
    mov     r9, r8
    shr     r9, 3
    test    r9, r9
    jz      @crc_tail

    ; Align/stream QWORDs
    mov     r10, rax
@crc_qword_loop:
    crc32   r10, qword ptr [rdx]
    add     rdx, 8
    dec     r9
    jnz     @crc_qword_loop
    mov     eax, r10d

@crc_tail:
    ; Process remaining 0..7 bytes
    and     r8, 7
    test    r8, r8
    jz      @crc_done

@crc_byte_loop:
    crc32   eax, byte ptr [rdx]
    inc     rdx
    dec     r8
    jnz     @crc_byte_loop

@crc_done:
    ret
UnpdComputeCrc32_Buffer ENDP

;------------------------------------------------------------------------------
; CR0 Write-Protect (WP) Bit Toggles (Bit 16: 0x10000)
;------------------------------------------------------------------------------
UnpdDisableWriteProtect PROC
    mov     rax, cr0
    and     rax, NOT (10000h)
    mov     cr0, rax
    ret
UnpdDisableWriteProtect ENDP

UnpdEnableWriteProtect PROC
    mov     rax, cr0
    or      rax, 10000h
    mov     cr0, rax
    ret
UnpdEnableWriteProtect ENDP

;------------------------------------------------------------------------------
; Hardware Cache Line Flushing & Write-Back
;------------------------------------------------------------------------------
UnpdClflush PROC
    clflush byte ptr [rcx]
    ret
UnpdClflush ENDP

UnpdClflushopt PROC
    clflushopt byte ptr [rcx]
    ret
UnpdClflushopt ENDP

UnpdClwb PROC
    clwb    byte ptr [rcx]
    ret
UnpdClwb ENDP

;------------------------------------------------------------------------------
; CPU Execution & Synchronization Primitives
;------------------------------------------------------------------------------
UnpdPause PROC
    pause
    ret
UnpdPause ENDP

UnpdCli PROC
    cli
    ret
UnpdCli ENDP

UnpdSti PROC
    sti
    ret
UnpdSti ENDP

UnpdSwapGs PROC
    swapgs
    ret
UnpdSwapGs ENDP

UnpdVmxoff PROC
    vmxoff
    ret
UnpdVmxoff ENDP

;------------------------------------------------------------------------------
; Hardware I/O Port Access Primitives
;------------------------------------------------------------------------------
UnpdInByte PROC
    mov     dx, cx
    xor     rax, rax
    in      al, dx
    ret
UnpdInByte ENDP

UnpdOutByte PROC
    ; RCX = port, DL = value
    mov     rax, rdx
    mov     dx, cx
    out     dx, al
    ret
UnpdOutByte ENDP

UnpdInWord PROC
    mov     dx, cx
    xor     rax, rax
    in      ax, dx
    ret
UnpdInWord ENDP

UnpdOutWord PROC
    mov     ax, dx
    mov     dx, cx
    out     dx, ax
    ret
UnpdOutWord ENDP

UnpdInDword PROC
    mov     dx, cx
    xor     rax, rax
    in      eax, dx
    ret
UnpdInDword ENDP

UnpdOutDword PROC
    mov     eax, edx
    mov     dx, cx
    out     dx, eax
    ret
UnpdOutDword ENDP

END
