[bits 32]

setup_identity_paging:
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 4096
    rep stosd

    mov dword [0x1000], 0x2003
    mov dword [0x2000], 0x3003

    mov edi, 0x3000
    mov ebx, 0x00000083
    mov ecx, 512
.map_loop:
    mov [edi], ebx
    add ebx, 0x200000
    add edi, 8
    loop .map_loop
    ret
