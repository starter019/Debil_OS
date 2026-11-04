; DEBIL OS - 32-bit kernel entry: GDT, stack, BSS, paging, C entry
BITS 32

SECTION .text.entry
GLOBAL _start
EXTERN _kernel_main
EXTERN __bss_start
EXTERN __bss_end

_start:
    cli
    lgdt [gdt_descriptor]
    jmp 0x08:.reload

.reload:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Zero the kernel BSS.
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    ; Build a page directory at 0x3000: identity map the first 256 MB
    ; with 4 MB pages, then enable PSE + paging. This gives us real page
    ; faults for the RAM/GPU crash tests.
    mov edi, 0x3000
    xor eax, eax
    mov ecx, 1024
    rep stosd

    mov edi, 0x3000
    mov eax, 0x00000083          ; present | writable | 4 MB page
    mov ecx, 64                  ; 64 * 4 MB = 256 MB
.map_loop:
    mov [edi], eax
    add eax, 0x00400000
    add edi, 4
    loop .map_loop

    mov eax, 0x3000
    mov cr3, eax

    mov eax, cr4
    or eax, 0x00000010           ; CR4.PSE
    mov cr4, eax

    mov eax, cr0
    or eax, 0x80000000           ; CR0.PG
    mov cr0, eax
    jmp .paging_on
.paging_on:

    call _kernel_main

.hang:
    cli
    hlt
    jmp .hang

; ------------------------------------------------------------------
SECTION .data
ALIGN 8

gdt_start:
    dq 0x0000000000000000        ; null
    dq 0x00CF9A000000FFFF        ; 0x08: 32-bit code, base 0, limit 4 GB
    dq 0x00CF92000000FFFF        ; 0x10: data,          base 0, limit 4 GB
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

