bits 64
section .text
global enter_user_mode

; void enter_user_mode(uint64_t entry /*rdi*/, uint64_t user_rsp /*rsi*/)
;
; There is no instruction that says "drop to ring 3". The only way down is to
; return there: iretq pops RIP, CS, RFLAGS, RSP and SS, and if the new CS has
; a lower privilege than the current one, the CPU switches rings. So we build
; the frame an interrupt from ring 3 WOULD have pushed, and "return" to it.
enter_user_mode:
    cli

    mov ax, 0x1B              ; user data selector (index 3, RPL 3)
    mov ds, ax
    mov es, ax

    push qword 0x1B           ; SS
    push rsi                  ; RSP: top of the user stack
    push qword 0x202          ; RFLAGS: IF=1 so the timer can preempt us, bit 1 is reserved-1
    push qword 0x23           ; CS: user code selector (index 4, RPL 3)
    push rdi                  ; RIP: the program's entry point

    ; Do not leak kernel register values to user mode.
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor r15d, r15d

    iretq
