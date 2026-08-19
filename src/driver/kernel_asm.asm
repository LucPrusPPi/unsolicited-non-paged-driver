;------------------------------------------------------------------------------
; UNPD Kernel Assembly Routines (x64 MASM)
; Low-level hardware memory barriers, high-resolution cycle timestamping,
; and architectural control register access.
;------------------------------------------------------------------------------

.code

; uint64_t UnpdReadTsc(void)
UnpdReadTsc PROC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    ret
UnpdReadTsc ENDP

; void UnpdMemoryFence(void)
UnpdMemoryFence PROC
    mfence
    ret
UnpdMemoryFence ENDP

; void UnpdFastSwapBarrier(void* activePtr, void* standbyPtr)
UnpdFastSwapBarrier PROC
    ; RCX = activePtr, RDX = standbyPtr
    mfence
    mov     rax, [rdx]
    xchg    [rcx], rax
    mov     [rdx], rax
    mfence
    ret
UnpdFastSwapBarrier ENDP

; void UnpdCpuId(int cpuInfo[4], int functionId)
UnpdCpuId PROC
    ; RCX = cpuInfo pointer, EDX = functionId
    push    rbx
    push    rdi
    mov     rdi, rcx
    mov     eax, edx
    xor     ecx, ecx
    cpuid
    mov     [rdi], eax
    mov     [rdi+4], ebx
    mov     [rdi+8], ecx
    mov     [rdi+12], edx
    pop     rdi
    pop     rbx
    ret
UnpdCpuId ENDP

; uint64_t UnpdReadCr0(void)
UnpdReadCr0 PROC
    mov     rax, cr0
    ret
UnpdReadCr0 ENDP

; void UnpdWriteCr0(uint64_t val)
UnpdWriteCr0 PROC
    mov     cr0, rcx
    ret
UnpdWriteCr0 ENDP

; uint64_t UnpdReadCr4(void)
UnpdReadCr4 PROC
    mov     rax, cr4
    ret
UnpdReadCr4 ENDP

; void UnpdWriteCr4(uint64_t val)
UnpdWriteCr4 PROC
    mov     cr4, rcx
    ret
UnpdWriteCr4 ENDP

; uint64_t UnpdReadMsr(uint32_t msr)
UnpdReadMsr PROC
    ; ECX = msr id
    rdmsr
    shl     rdx, 32
    or      rax, rdx
    ret
UnpdReadMsr ENDP

; void UnpdWriteMsr(uint32_t msr, uint64_t value)
UnpdWriteMsr PROC
    ; ECX = msr id, RDX = value
    mov     rax, rdx
    shr     rdx, 32
    wrmsr
    ret
UnpdWriteMsr ENDP

END
