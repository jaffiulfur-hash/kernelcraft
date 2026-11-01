section .multiboot
align 8
mb2_header:
    dd 0xE85250D6
    dd 0
    dd mb2_end - mb2_header
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_header))

    dw 0
    dw 0
    dd 8
mb2_end:

section .bss
alignb 16
stack_bottom:
    resb 16384
stack_top:

alignb 4
mb2_magic_saved:
    resd 1
mb2_info_saved:
    resd 1

alignb 4096
pml4_table:
    resq 512

alignb 4096
pdpt_table:
    resq 512

alignb 4096
pd_table:
    resq 512

section .rodata
align 8
gdt64:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

section .text
global _start
extern kmain

bits 32
_start:
    cli

    ; Multiboot2 passes:
    ; eax = magic
    ; ebx = info structure physical address
    mov [mb2_magic_saved], eax
    mov [mb2_info_saved], ebx

    mov esp, stack_top

    ; Zero our temporary paging structures
    xor eax, eax
    mov edi, pml4_table
    mov ecx, (4096 * 3) / 4
    rep stosd

    ; PML4[0] -> PDPT
    mov eax, pdpt_table
    or eax, 0x03
    mov [pml4_table], eax

    ; PDPT[0] -> PD
    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_table], eax

    ; Identity map first 1 GiB using 2 MiB pages
    xor ecx, ecx
.map_pd:
    mov eax, ecx
    shl eax, 21
    or eax, 0x83                  ; present | writable | huge
    mov [pd_table + ecx * 8], eax
    mov dword [pd_table + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .map_pd

    ; Load temporary 64-bit GDT
    lgdt [gdt64_ptr]

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Load PML4 into CR3
    mov eax, pml4_table
    mov cr3, eax

    ; Enable long mode in EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging + keep protected mode enabled
    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax

    ; Far jump into 64-bit code
    jmp 0x08:long_mode_entry

bits 64
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top
    mov rbp, rsp

    ; kmain(uint32_t mb_magic, uint32_t mb_info_addr)
    mov edi, dword [mb2_magic_saved]
    mov esi, dword [mb2_info_saved]
    call kmain

.hang:
    cli
    hlt
    jmp .hang
