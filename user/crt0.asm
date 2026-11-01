bits 64
section .text
global _start
extern main

; Every user program starts here. The kernel enters at _start in ring 3 with a
; fresh stack; we call main() and turn its return value into an exit system
; call (number 5: SYS_EXIT, code in rdi).
_start:
    call main
    mov edi, eax
    mov eax, 5
    int 0x80
.hang:
    ud2                       ; exit never returns; trap if it somehow does
    jmp .hang
