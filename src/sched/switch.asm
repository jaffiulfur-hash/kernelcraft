bits 64
section .text

global context_switch

; void context_switch(uint64_t *save_old_rsp, uint64_t new_rsp)
;                     rdi                      rsi
;
; The System V ABI says a function may freely clobber rax, rcx, rdx, rsi, rdi
; and r8-r11, but must preserve rbx, rbp and r12-r15. So when thread A calls
; this, the compiler has already saved everything it cares about; we only
; have to save those six "callee-saved" registers, swap stacks, and restore
; the other thread's six. The final `ret` then pops the return address that
; sits on the NEW stack - which is where that thread last called us from,
; or, for a brand-new thread, thread_trampoline.
context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi], rsp        ; remember where A's stack ended
    mov rsp, rsi          ; from here on we are on B's stack

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
