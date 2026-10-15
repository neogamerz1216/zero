[bits 64]
global _start
extern kernel_main
extern __bss_start
extern __bss_end

section .text
_start:
    cld
    and rsp, 0xFFFFFFFFFFFFFFF0

    ; Zero the .bss region. Real hardware does not guarantee zeroed RAM,
    ; and .bss is not part of the loaded image.
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    add rcx, 7
    shr rcx, 3
    xor eax, eax
    rep stosq

    call kernel_main
    cli
.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
