/*
 * Linker script for user programs. Each one is a static ELF executable
 * placed at USER_BASE (see include/mm/aspace.h). Code, read-only data and
 * data/bss go in separate page-aligned segments so the kernel can give each
 * its own permissions: r-x, r--, rw- (and NX on the last two).
 */
ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5);   /* r-x */
    rodata PT_LOAD FLAGS(4);   /* r-- */
    data   PT_LOAD FLAGS(6);   /* rw- */
}

SECTIONS
{
    . = 0x40000000;

    .text : ALIGN(4K) { *(.text*) } :text

    .rodata : ALIGN(4K) { *(.rodata*) } :rodata

    .data : ALIGN(4K)
    {
        *(.data*)
        *(.bss*)
        *(COMMON)
    } :data

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
