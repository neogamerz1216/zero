/* ZeroOS kernel linker script
 * Flat binary at 1 MiB, no debug/symbol sections, exposes BSS bounds.
 */
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
    usertext PT_LOAD FLAGS(5);
    userrodata PT_LOAD FLAGS(4);
}

SECTIONS
{
    . = 0x100000;

    .text : {
        kernel/kernel_entry.o(.text .text.*);
        *(EXCLUDE_FILE (apps/store_apps.o apps/user_abi.o) .text .text.*);
    } :text

    . = ALIGN(0x1000);
    .rodata : {
        *(EXCLUDE_FILE (apps/store_apps.o apps/user_abi.o) .rodata .rodata.*);
    } :rodata

    . = ALIGN(0x1000);
    .data : {
        *(EXCLUDE_FILE (apps/store_apps.o apps/user_abi.o) .data .data.*);
    } :data

    . = ALIGN(0x1000);
    __user_text_start = .;
    .user_text : {
        apps/store_apps.o(.text .text.*);
        apps/user_abi.o(.text .text.* .user_text .user_text.*);
    } :usertext
    __user_text_end = .;

    . = ALIGN(0x1000);
    __user_rodata_start = .;
    .user_rodata : {
        apps/store_apps.o(.rodata .rodata.*)
        apps/user_abi.o(.rodata .rodata.*)
    } :userrodata
    __user_rodata_end = .;

    . = ALIGN(0x1000);
    __bss_start = .;
    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data
    . = ALIGN(16);
    __bss_end = .;
    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.symtab)
        *(.strtab)
        *(.shstrtab)
    }
}
