; DEBIL OS - 512-byte MBR boot sector
; Loads the flat kernel binary from LBA 1 to 0x10000, collects an E820 map,
; enters 32-bit protected mode, copies the kernel to 0x100000 and jumps to it.

BITS 16
ORG 0x7C00

KERNEL_LBA      equ 1
KERNEL_SECTORS  equ 448          ; 224 KB maximum kernel size
KERNEL_LOAD_SEG equ 0x1000       ; load at 0x10000 in real mode
KERNEL_DEST     equ 0x100000
BOOT_INFO       equ 0x7F00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_boot
    call print

    call enable_a20
    call detect_memory
    call load_kernel

    mov si, msg_ok
    call print

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:protected_mode

; ------------------------------------------------------------------
; print a zero terminated string in DS:SI with BIOS teletype
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
    ; Fast A20 gate
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al
    popa
    ret

; ------------------------------------------------------------------
; E820 memory map into 0x8000, boot info at 0x7F00
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

    ; E820 unavailable: fall back to INT 15h AX=E801
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
; Read KERNEL_SECTORS from LBA 1 using INT 13h extensions.
load_kernel:
    pusha

    mov word [load_seg], KERNEL_LOAD_SEG
    mov dword [dap_lba], KERNEL_LBA
    mov cx, KERNEL_SECTORS

.loop:
    mov ax, 64
    cmp cx, 64
    jae .chunk
    mov ax, cx
.chunk:
    mov [dap_count], ax

    mov es, [load_seg]
    mov ax, [load_seg]
    mov [dap_seg], ax
    xor bx, bx
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .error

    movzx eax, word [dap_count]
    add dword [dap_lba], eax

    mov dx, ax
    shl dx, 5                   ; sectors * 512 / 16 paragraphs
    add [load_seg], dx

    sub cx, ax
    jnz .loop

    popa
    ret

.error:
    mov si, msg_disk_error
    call print
    cli
.hang:
    hlt
    jmp .hang

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
    mov esi, 0x10000
    mov edi, KERNEL_DEST
    mov ecx, (KERNEL_SECTORS * 512) / 4
    rep movsd

    jmp KERNEL_DEST

; ------------------------------------------------------------------
BITS 16

align 4
dap:
    db 0x10
    db 0x00
dap_count:
    dw 64
    dw 0x0000
dap_seg:
    dw KERNEL_LOAD_SEG
dap_lba:
    dq KERNEL_LBA

load_seg:   dw KERNEL_LOAD_SEG
boot_drive: db 0

msg_boot:       db "DEBIL boot...", 13, 10, 0
msg_ok:         db "PM...", 13, 10, 0
msg_disk_error: db "Disk err", 13, 10, 0

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
