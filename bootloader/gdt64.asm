[bits 16]

gdt32_start:
    dq 0x0000000000000000
gdt32_code:
    dw 0xffff, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
gdt32_data:
    dw 0xffff, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt32_end:

gdt32_descriptor:
    dw gdt32_end - gdt32_start - 1
    dd gdt32_start

CODE32_SEG equ gdt32_code - gdt32_start
DATA32_SEG equ gdt32_data - gdt32_start

align 8
gdt64_start:
    dq 0x0000000000000000

gdt64_code:
    dw 0x0000, 0x0000
    db 0x00, 10011010b, 00100000b, 0x00

gdt64_data:
    dw 0x0000, 0x0000
    db 0x00, 10010010b, 00000000b, 0x00

gdt64_user_code:
    dw 0x0000, 0x0000
    db 0x00, 11111010b, 00100000b, 0x00

gdt64_user_data:
    dw 0x0000, 0x0000
    db 0x00, 11110010b, 00000000b, 0x00

gdt64_user_sysret_code:
    dw 0x0000, 0x0000
    db 0x00, 11111010b, 00100000b, 0x00

gdt64_tss:
    dw 103
    dw 0x6000
    db 0x00, 10001001b, 00000000b, 0x00
    dd 0x00000000
    dd 0x00000000

gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start

CODE64_SEG equ gdt64_code - gdt64_start
DATA64_SEG equ gdt64_data - gdt64_start
USER64_CODE_SEG equ gdt64_user_code - gdt64_start
USER64_DATA_SEG equ gdt64_user_data - gdt64_start
USER64_SYSRET_CODE_SEG equ gdt64_user_sysret_code - gdt64_start
TSS64_SEG  equ gdt64_tss - gdt64_start
