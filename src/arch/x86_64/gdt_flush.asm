bits 64
section .text
global gdt_flush

gdt_flush:
    lgdt [rdi]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload CS with a far return: push selector, then target RIP, then retfq.
    ; (NASM spells the 64-bit far return "retfq"; "lretq" is the GNU as name
    ; and NASM would silently treat it as a label.)
    push 0x08
    lea rax, [rel gdt_flush_reload_cs]
    push rax
    retfq

gdt_flush_reload_cs:
    ret
