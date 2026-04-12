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

    push 0x08
    lea rax, [rel gdt_flush_reload_cs]
    push rax
    lretq

gdt_flush_reload_cs:
    ret
