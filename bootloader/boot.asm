[org 0x7c00]
[bits 16]

STAGE2_OFFSET  equ 0x7e00
STAGE2_LBA     equ 1
STAGE2_SECTORS equ 8

start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov sp, 0x7c00
    sti

    mov [BOOT_DRIVE], dl

    mov si, MSG_WATERMARK
    call print_string_16

    call check_disk_extensions

    mov eax, STAGE2_LBA
    mov cx, STAGE2_SECTORS
    xor bx, bx
    mov es, bx
    mov bx, STAGE2_OFFSET
    call read_sectors_lba

    mov dl, [BOOT_DRIVE]
    jmp 0x0000:STAGE2_OFFSET

print_string_16:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0e
    mov bx, 0x0007
    int 0x10
    jmp .loop
.done:
    popa
    ret

%include "disk.asm"

BOOT_DRIVE:        db 0
MSG_NO_EXTENSIONS: db "Err: No LBA", 0x0d, 0x0a, 0
MSG_DISK_ERROR:    db "Err: Read", 0x0d, 0x0a, 0
MSG_WATERMARK:     db "Enhanced By ZeroOS", 0x0d, 0x0a, 0

times 510 - ($ - $$) db 0
dw 0xaa55
