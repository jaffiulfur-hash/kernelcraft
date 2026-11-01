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
    __multiboot_end = .;

    .text : ALIGN(4K)
    {
        __text_start = .;
        *(.text*)
        __text_end = .;
    }

    .rodata : ALIGN(4K)
    {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    }

    .data : ALIGN(4K)
    {
        __data_start = .;
        *(.data*)
        __data_end = .;
    }

    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
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
