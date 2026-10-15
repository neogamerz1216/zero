[bits 16]

check_disk_extensions:
    push bx
    push dx
    mov ah, 0x41
    mov bx, 0x55aa
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc .fallback
    cmp bx, 0xaa55
    jne .fallback
    pop dx
    pop bx
    ret

.fallback:
    pop dx
    pop bx
    mov si, MSG_NO_EXTENSIONS
    call print_string_16
.hang:
    hlt
    jmp .hang

; Reads CX sectors from EAX LBA into ES:BX, with resets and retries.
read_sectors_lba:
    push si
    push ax
    push dx
    push cx
    push bx

    mov [dap_lba_low], eax
    mov [dap_count], cx
    mov [dap_offset], bx
    mov ax, es
    mov [dap_segment], ax

    mov bx, 4                    ; retry attempts
.retry:
    push bx
    mov si, disk_address_packet
    mov dl, [BOOT_DRIVE]
    mov ah, 0x42
    int 0x13
    pop bx
    jnc .ok

    push bx
    xor ah, ah                   ; reset disk system
    mov dl, [BOOT_DRIVE]
    int 0x13
    pop bx

    dec bx
    jnz .retry

    mov si, MSG_DISK_ERROR
    call print_string_16
.error_hang:
    hlt
    jmp .error_hang

.ok:
    pop bx
    pop cx
    pop dx
    pop ax
    pop si
    ret

align 4
disk_address_packet:
    db 0x10
    db 0x00
dap_count:    dw 0
dap_offset:   dw 0
dap_segment:  dw 0
dap_lba_low:  dd 0
dap_lba_high: dd 0
