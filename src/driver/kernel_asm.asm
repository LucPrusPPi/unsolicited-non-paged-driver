;==============================================================================
; UNPD Kernel Assembly Subsystem (x64 MASM)
; Provides hardware serialization, high-resolution cycle timestamping,
; fast string streaming memory operations, ring-0 architectural register
; manipulation, and MMU/TLB invalidation primitives.
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
    ; RCX = pointer to uint32_t aux
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
    ; RCX = activePtr, RDX = standbyPtr
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
    ; RCX = destination, RDX = source, R8 = qwordCount
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
    ; RCX = destination, RDX = qwordCount
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
    ; RCX = cpuInfo array pointer, EDX = functionId
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
; Returns the CR0 control register value.
;------------------------------------------------------------------------------
UnpdReadCr0 PROC
    mov     rax, cr0
    ret
UnpdReadCr0 ENDP

;------------------------------------------------------------------------------
; void UnpdWriteCr0(uint64_t value)
; Sets the CR0 control register.
;------------------------------------------------------------------------------
UnpdWriteCr0 PROC
    mov     cr0, rcx
    ret
UnpdWriteCr0 ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadCr2(void)
; Returns the CR2 page-fault linear address register.
;------------------------------------------------------------------------------
UnpdReadCr2 PROC
    mov     rax, cr2
    ret
UnpdReadCr2 ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadCr3(void)
; Returns the CR3 page directory base register (PML4 pointer).
;------------------------------------------------------------------------------
UnpdReadCr3 PROC
    mov     rax, cr3
    ret
UnpdReadCr3 ENDP

;------------------------------------------------------------------------------
; void UnpdWriteCr3(uint64_t value)
; Sets the CR3 page directory base register, invalidating non-global TLB entries.
;------------------------------------------------------------------------------
UnpdWriteCr3 PROC
    mov     cr3, rcx
    ret
UnpdWriteCr3 ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadCr4(void)
; Returns the CR4 control register value.
;------------------------------------------------------------------------------
UnpdReadCr4 PROC
    mov     rax, cr4
    ret
UnpdReadCr4 ENDP

;------------------------------------------------------------------------------
; void UnpdWriteCr4(uint64_t value)
; Sets the CR4 control register.
;------------------------------------------------------------------------------
UnpdWriteCr4 PROC
    mov     cr4, rcx
    ret
UnpdWriteCr4 ENDP

;------------------------------------------------------------------------------
; void UnpdInvlpg(const void* virtualAddress)
; Invalidates the TLB mapping for the specified virtual address.
;------------------------------------------------------------------------------
UnpdInvlpg PROC
    invlpg  byte ptr [rcx]
    ret
UnpdInvlpg ENDP

;------------------------------------------------------------------------------
; void UnpdWbinvd(void)
; Flushes internal CPU caches and writes back all modified cache lines.
;------------------------------------------------------------------------------
UnpdWbinvd PROC
    wbinvd
    ret
UnpdWbinvd ENDP

;------------------------------------------------------------------------------
; void UnpdFlushTlb(void)
; Reloads CR3 to flush all non-global TLB translation entries.
;------------------------------------------------------------------------------
UnpdFlushTlb PROC
    mov     rax, cr3
    mov     cr3, rax
    ret
UnpdFlushTlb ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdReadMsr(uint32_t msr)
; Reads 64-bit Model-Specific Register.
;------------------------------------------------------------------------------
UnpdReadMsr PROC
    ; ECX = msr id
    rdmsr
    shl     rdx, 32
    or      rax, rdx
    ret
UnpdReadMsr ENDP

;------------------------------------------------------------------------------
; void UnpdWriteMsr(uint32_t msr, uint64_t value)
; Writes 64-bit Model-Specific Register.
;------------------------------------------------------------------------------
UnpdWriteMsr PROC
    ; ECX = msr id, RDX = 64-bit value
    mov     rax, rdx
    shr     rdx, 32
    wrmsr
    ret
UnpdWriteMsr ENDP

;------------------------------------------------------------------------------
; uint64_t UnpdGetRflags(void)
; Returns current RFLAGS register.
;------------------------------------------------------------------------------
UnpdGetRflags PROC
    pushfq
    pop     rax
    ret
UnpdGetRflags ENDP

;------------------------------------------------------------------------------
; void UnpdSetRflags(uint64_t flags)
; Sets the RFLAGS register.
;------------------------------------------------------------------------------
UnpdSetRflags PROC
    push    rcx
    popfq
    ret
UnpdSetRflags ENDP

END
