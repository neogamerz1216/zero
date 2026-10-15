ASM = nasm
CC = gcc
LD = ld

CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mcmodel=large -Wall -Wextra -O2
LDFLAGS = -m elf_x86_64 -T linker.ld -z noexecstack

C_SOURCES = $(wildcard kernel/*.c drivers/*.c shell/*.c fs/*.c apps/*.c)
C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_SOURCES = kernel/kernel_entry.asm kernel/context_switch.asm kernel/interrupts.asm kernel/syscall_entry.asm
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)

all: zeroos.img

zeroos.img: bootloader/boot.bin bootloader/stage2.bin kernel.bin
	cat bootloader/boot.bin bootloader/stage2.bin kernel.bin > zeroos.img
	truncate -s 33554432 zeroos.img

bootloader/boot.bin: bootloader/boot.asm bootloader/disk.asm
	$(ASM) -f bin -i bootloader/ bootloader/boot.asm -o bootloader/boot.bin

bootloader/stage2.bin: bootloader/stage2.asm bootloader/disk.asm bootloader/memory.asm bootloader/a20.asm bootloader/cpuid.asm bootloader/paging.asm bootloader/gdt64.asm
	$(ASM) -f bin -i bootloader/ bootloader/stage2.asm -o bootloader/stage2.bin

kernel.bin: $(ASM_OBJECTS) $(C_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o kernel.elf $(ASM_OBJECTS) $(C_OBJECTS)
	objcopy -O binary kernel.elf kernel.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

apps/store_apps.o: apps/store_apps.c apps/zap.h
	$(CC) $(CFLAGS) -fno-merge-constants -c $< -o $@

%.o: %.asm
	$(ASM) -f elf64 $< -o $@

run: zeroos.img
	qemu-system-x86_64 -drive file=zeroos.img,format=raw,index=0,media=disk -m 256M -serial stdio

clean:
	rm -f bootloader/*.bin kernel/*.o drivers/*.o shell/*.o fs/*.o apps/*.o *.bin *.elf zeroos.img
