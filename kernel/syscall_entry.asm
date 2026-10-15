[bits 64]

global syscall_entry
global enter_user_process
global restore_kernel_context
extern syscall_dispatch

section .bss align=16
syscall_stack: resb 16384
syscall_stack_top:
user_saved_kernel_rsp: resq 1

section .text

; syscall ABI: rax=num, rdi=a, rsi=b, rdx=c. rcx=return rip, r11=return rflags.
syscall_entry:
    mov [rel saved_user_rsp_tmp], rsp
    lea rsp, [rel syscall_stack_top]

    push rcx
    push r11
    push rdx
    push rsi
    push rdi

    mov rdi, rax
    mov rsi, [rsp]
    mov rdx, [rsp + 8]
    mov rcx, [rsp + 16]
    call syscall_dispatch

    add rsp, 24
    pop r11
    pop rcx
    mov rsp, [rel saved_user_rsp_tmp]
    sysret

section .bss align=16
saved_user_rsp_tmp: resq 1

section .text

; rdi=user rip (trampoline), rsi=user rsp, rdx=user rdi (api),
; rcx=root phys, r8=irq stack top, r9=app entry (user rsi)
enter_user_process:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rel user_saved_kernel_rsp], rsp
    mov rax, 0x6004
    mov [rax], r8
    mov rax, rcx
    mov cr3, rax

    push qword 0x23
    push rsi
    push qword 0x202
    push qword 0x1b
    push rdi
    mov rdi, rdx
    mov rsi, r9
    iretq

restore_kernel_context:
    mov rsp, [rel user_saved_kernel_rsp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
