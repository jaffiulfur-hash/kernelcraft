ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* First byte of the kernel image (physical == virtual, identity mapped). */
    __kernel_start = .;

    .multiboot : ALIGN(8)
    {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    /* One byte past the end of the kernel image, rounded up to a frame. */
    . = ALIGN(4K);
    __kernel_end = .;

    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment)
    }
}
