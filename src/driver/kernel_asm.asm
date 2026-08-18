;------------------------------------------------------------------------------
; UNPD Kernel Assembly Routines (x64 MASM)
; Low-level hardware memory barriers and high-resolution cycle timestamping.
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

END
