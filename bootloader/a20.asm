[bits 16]

; Robust A20 enable. Tries BIOS, then Fast A20 (port 0x92),
; then the keyboard-controller method. Verifies with a memory wrap test.

enable_a20:
    ; Method 1: BIOS
    mov ax, 0x2401
    int 0x15
    call a20_check
    jc .done

    ; Method 2: Fast A20 gate
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al
    call a20_check
    jc .done

    ; Method 3: Keyboard controller
    call a20_kbd
    call a20_check
.done:
    ret

; Returns CF=1 if A20 is enabled.
a20_check:
    push ds
    push es
    pusha
    cli
    xor ax, ax
    mov ds, ax
    mov si, 0x0500
    not ax                      ; 0xFFFF -> segment 0x100000+0x510
    mov es, ax
    mov di, 0x0510
    mov al, [ds:si]
    push ax
    mov al, [es:di]
    push ax
    mov byte [ds:si], 0x00
    mov byte [es:di], 0xFF
    cmp byte [ds:si], 0xFF      ; wrapped? A20 off
    je .off
    pop ax
    mov [es:di], al
    pop ax
    mov [ds:si], al
    sti
    popa
    pop es
    pop ds
    stc
    ret
.off:
    pop ax
    mov [es:di], al
    pop ax
    mov [ds:si], al
    sti
    popa
    pop es
    pop ds
    clc
    ret

a20_kbd:
    call a20_wait_out
    mov al, 0xAD
    out 0x64, al
    call a20_wait_out
    mov al, 0xD0
    out 0x64, al
    call a20_wait_in
    in al, 0x60
    push ax
    call a20_wait_out
    mov al, 0xD1
    out 0x64, al
    call a20_wait_out
    pop ax
    or al, 2
    out 0x60, al
    call a20_wait_out
    mov al, 0xAE
    out 0x64, al
    call a20_wait_out
    ret

a20_wait_out:
    in al, 0x64
    test al, 2
    jnz a20_wait_out
    ret

a20_wait_in:
    in al, 0x64
    test al, 1
    jz a20_wait_in
    ret
