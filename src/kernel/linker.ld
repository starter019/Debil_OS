/* DEBIL OS kernel link script.
 * The kernel is a flat binary loaded at physical 0x100000 by the boot sector.
 * We use PE linking (MinGW binutils has no ELF target) but objcopy strips the
 * PE headers, and section VMAs are absolute physical addresses. */
ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text : {
        *(.text.entry)
        *(.text*)
    }

    .rdata : {
        *(.rdata*)
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        . = ALIGN(4096);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4096);
        __bss_end = .;
    }

    _kernel_end = .;

    /DISCARD/ : {
        *(.reloc)
        *(.eh_frame*)
        *(.comment*)
        *(.debug*)
    }
}
