[org 0x7e00]
[bits 16]

KERNEL_START_LBA equ 9
KERNEL_SECTORS   equ 128

stage2_entry:
    mov [BOOT_DRIVE], dl

    call detect_memory
    call enable_a20

    mov eax, KERNEL_START_LBA
    mov cx, KERNEL_SECTORS
    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    call read_sectors_lba

    xor ax, ax
    mov es, ax

    cli
    lgdt [gdt32_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE32_SEG:pm_entry

print_string_16:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp .loop
.done:
    popa
    ret

%include "disk.asm"
%include "memory.asm"
%include "a20.asm"
%include "gdt64.asm"

BOOT_DRIVE:        db 0
MSG_NO_EXTENSIONS: db "LBA unsupported", 0
MSG_DISK_ERROR:    db "Disk read error", 0

[bits 32]
pm_entry:
    mov ax, DATA32_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov esp, 0x90000

    call check_cpuid
    call check_long_mode
    call setup_identity_paging

    mov eax, 0x1000
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_descriptor]
    jmp CODE64_SEG:long_mode_entry

%include "cpuid.asm"
%include "paging.asm"

[bits 64]
long_mode_entry:
    mov ax, DATA64_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, 0x1FFFF0
    mov rbp, rsp

    mov rdi, 0x6000
    xor rax, rax
    mov rcx, 13
    rep stosq

    mov rax, 0x6004
    mov qword [rax], 0x1FFFF0

    mov ax, TSS64_SEG
    ltr ax

    mov rsi, 0x10000
    mov rdi, 0x100000
    mov rcx, (KERNEL_SECTORS * 512) / 8
    rep movsq

    mov rax, 0x100000
    jmp rax

times 4096 - ($ - $$) db 0
