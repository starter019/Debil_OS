; DEBIL OS - El Torito no-emulation CD boot loader
;
; The ISO's El Torito boot image is just this 512-byte stage. The kernel is
; stored at a known CD LBA (KERNEL_LBA), so the stage reads it with INT 13h
; EDD (2048-byte CD sectors), copies it to 0x100000 and jumps to it.
;
; KERNEL_LBA and KERNEL_BYTES are supplied by build.ps1 via NASM -D options.

BITS 16
ORG 0x7C00

KERNEL_STAGE        equ 0x10000
KERNEL_DEST         equ 0x100000
BOOT_INFO           equ 0x7F00
KERNEL_CD_SECTORS   equ (KERNEL_BYTES + 2047) / 2048
KERNEL_COPY_DWORDS  equ (KERNEL_BYTES + 3) / 4

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    call detect_memory
    call read_kernel
    call enable_a20

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:protected_mode

; ------------------------------------------------------------------
read_kernel:
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .error
    ret
.error:
    mov si, msg_read_error
    call print
    cli
.hang:
    hlt
    jmp .hang

; ------------------------------------------------------------------
print:
    pusha
.next:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .next
.done:
    popa
    ret

; ------------------------------------------------------------------
enable_a20:
    pusha
    mov ax, 0x2401
    int 0x15
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al
    popa
    ret

; ------------------------------------------------------------------
; E820 map into 0x8000, boot info at 0x7F00 (same as the disk loader).
detect_memory:
    pusha

    mov dword [BOOT_INFO + 0], 0xDEB11055
    mov dword [BOOT_INFO + 4], 0x8000
    mov dword [BOOT_INFO + 8], 0
    mov dword [BOOT_INFO + 12], 0
    mov dword [BOOT_INFO + 16], 0
    mov al, [boot_drive]
    mov [BOOT_INFO + 20], al

    xor ebx, ebx
    xor bp, bp
    mov di, 0x8000

.loop:
    xor ax, ax
    mov es, ax
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .done

    add di, 24
    inc bp
    cmp bp, 32
    jae .done
    test ebx, ebx
    jz .done
    jmp .loop

.done:
    movzx eax, bp
    mov [BOOT_INFO + 8], eax

    test bp, bp
    jnz .have_map

    mov ax, 0xE801
    int 0x15
    jc .have_map
    movzx eax, ax
    mov [BOOT_INFO + 12], eax
    movzx eax, bx
    shl eax, 6
    mov [BOOT_INFO + 16], eax

.have_map:
    popa
    ret

; ------------------------------------------------------------------
BITS 32
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    cld
    mov esi, KERNEL_STAGE
    mov edi, KERNEL_DEST
    mov ecx, KERNEL_COPY_DWORDS
    rep movsd

    jmp KERNEL_DEST

; ------------------------------------------------------------------
BITS 16

align 4
dap:
    db 0x10
    db 0x00
dap_count:
    dw KERNEL_CD_SECTORS
dap_offset:
    dw 0x0000
dap_segment:
    dw KERNEL_STAGE >> 4
dap_lba:
    dq KERNEL_LBA

boot_drive: db 0
msg_read_error: db "CD read error", 13, 10, 0

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
