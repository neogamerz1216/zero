[bits 16]

E820_MAP_LOCATION equ 0x5000
E820_COUNT_LOC    equ 0x7000

detect_memory:
    mov di, E820_MAP_LOCATION
    xor ebx, ebx
    mov word [e820_num], 0
.e820_loop:
    mov eax, 0xe820
    mov ecx, 24
    mov edx, 0x534D4150
    mov [es:di + 20], dword 1
    int 0x15
    jc .e820_finished
    cmp eax, 0x534D4150
    jne .e820_finished
    mov bp, [e820_num]
    cmp bp, 128
    jae .e820_finished
    inc word [e820_num]
    add di, 24
    test ebx, ebx
    jnz .e820_loop
.e820_finished:
    mov bp, [e820_num]
    mov [E820_COUNT_LOC], bp
    ret

e820_num: dw 0
