#include "user.h"
#include "mm.h"
#include "vm.h"
#include "../apps/zap.h"
#include "../drivers/serial.h"

#define TSS_RSP0_PHYS 0x6004ULL
#define BOOT_RSP0 0x1FFFF0ULL
#define USER_STACK_TOP 0x800000ULL
#define USER_STACK_SIZE 32768ULL
#define USER_HEAP_BASE 0x700000ULL
#define USER_HEAP_SIZE 0x80000ULL
#define USER_MAX_TABLES 128
#define USER_MAX_DATA_PAGES 256
#define USER_MAX_REGIONS 8

typedef struct {
    uint64_t base;
    uint64_t len;
    int read;
    int write;
} user_region_t;

extern char __user_text_start[];
extern char __user_text_end[];
extern char __user_rodata_start[];
extern char __user_rodata_end[];

extern void user_api_print(const char* str, uint8_t color);
extern void user_api_putc(char c, uint8_t color);
extern void user_api_clear(uint8_t bg);
extern char user_api_getchar(void);
extern void user_api_sleep(uint32_t ms);
extern void user_api_beep(uint32_t freq, uint32_t ms);
extern void user_api_set_cursor(int x, int y);

extern int enter_user_process(uint64_t rip, uint64_t rsp, uint64_t rdi, uint64_t root,
                              uint64_t irq_top, uint64_t app);
extern void restore_kernel_context(void);
extern void user_app_trampoline(const zap_api_t* api, zap_main_func app);

static int user_active = 0;
static uint64_t user_root = 0;
static uint64_t kernel_cr3_saved = 0;
static int user_exit_status = 0;
static unsigned long user_last_status = 0;
static const char* user_last_message = "ok";
static uint64_t user_brk = USER_HEAP_BASE;
static uint64_t user_tables[USER_MAX_TABLES];
static size_t user_table_count = 0;
static uint64_t user_data_pages[USER_MAX_DATA_PAGES];
static size_t user_data_count = 0;
static user_region_t user_regions[USER_MAX_REGIONS];
static int user_region_count = 0;
static uint8_t user_irq_stack[16384] __attribute__((aligned(16)));

static inline void tss_set_rsp0(uint64_t v) {
    *(volatile uint64_t*)(uintptr_t)TSS_RSP0_PHYS = v;
}

static int track_data(uint64_t phys) {
    if (user_data_count >= USER_MAX_DATA_PAGES) {
        return 0;
    }
    user_data_pages[user_data_count++] = phys;
    return 1;
}

static void add_region(uint64_t base, uint64_t len, int read, int write) {
    if (user_region_count < USER_MAX_REGIONS && len > 0) {
        user_regions[user_region_count].base = base;
        user_regions[user_region_count].len = len;
        user_regions[user_region_count].read = read;
        user_regions[user_region_count].write = write;
        user_region_count++;
    }
}

static int in_stack_range(uint64_t addr) {
    return addr >= USER_STACK_TOP - USER_STACK_SIZE && addr < USER_STACK_TOP;
}

int user_process_active(void) {
    return user_active;
}

int user_memory_readable(uint64_t addr, size_t len) {
    if (!user_active || len == 0 || len > 8192) {
        return 0;
    }
    if (addr + len < addr) {
        return 0;
    }
    for (int i = 0; i < user_region_count; i++) {
        if (user_regions[i].read &&
            addr >= user_regions[i].base &&
            addr + len <= user_regions[i].base + user_regions[i].len) {
            return 1;
        }
    }
    return 0;
}

int user_handle_page_fault(uint64_t addr, uint64_t err) {
    (void)err;
    if (!user_active || !user_root) {
        return 0;
    }
    uint64_t page = addr & ~0xFFFULL;
    int stack = in_stack_range(addr);
    int heap = addr >= USER_HEAP_BASE && addr < user_brk;
    if (!stack && !heap) {
        return 0;
    }
    uint64_t phys = pmm_alloc_page();
    if (!phys || !track_data(phys)) {
        if (phys) {
            pmm_free_page(phys);
        }
        return 0;
    }
    if (!vm_map_page_tracked(user_root, page, phys, VM_USER | VM_WRITE | VM_NX,
                             user_tables, &user_table_count, USER_MAX_TABLES)) {
        return 0;
    }
    return 1;
}

int user_copy_from_user(void* dst, uint64_t src, size_t len) {
    if (!dst || !user_active || !user_root || len == 0 || len > 8192) {
        return -1;
    }
    uint8_t* out = (uint8_t*)dst;
    for (size_t i = 0; i < len; i++) {
        uint64_t addr = src + i;
        uint64_t page = addr & ~0xFFFULL;
        if ((in_stack_range(addr) || (addr >= USER_HEAP_BASE && addr < user_brk)) &&
            !vm_page_present(user_root, page)) {
            if (!user_handle_page_fault(addr, 0)) {
                return -1;
            }
        }
        if (!user_memory_readable(addr, 1)) {
            return -1;
        }
        out[i] = *(volatile uint8_t*)(uintptr_t)addr;
    }
    return 0;
}

long user_process_set_brk(long new_brk) {
    if (!user_active) {
        return -1;
    }
    if (new_brk < (long)USER_HEAP_BASE || new_brk > (long)(USER_HEAP_BASE + USER_HEAP_SIZE)) {
        return (long)user_brk;
    }
    user_brk = (uint64_t)new_brk;
    for (int i = 0; i < user_region_count; i++) {
        if (user_regions[i].base == USER_HEAP_BASE) {
            user_regions[i].len = user_brk - USER_HEAP_BASE;
        }
    }
    return (long)user_brk;
}

void user_return_to_kernel(void) {
    vm_write_root(kernel_cr3_saved);
    tss_set_rsp0(BOOT_RSP0);
    user_active = 0;
    restore_kernel_context();
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

void user_process_exit(int status) {
    if (!user_active) {
        while (1) {
            __asm__ volatile ("cli; hlt");
        }
    }
    user_exit_status = status;
    user_last_status = (unsigned long)(uint32_t)status;
    user_last_message = "User process exited";
    user_return_to_kernel();
}

void user_handle_fatal(uint64_t num, uint64_t err, uint64_t addr) {
    (void)err;
    (void)addr;
    user_exit_status = -1;
    user_last_status = 0x80000000UL | (num & 0xFFUL);
    user_last_message = "User process terminated by fault";
    serial_puts("[ZeroOS] User fault isolated; kernel continues.\n");
    user_return_to_kernel();
}

unsigned long user_process_last_status(void) {
    return user_last_status;
}

const char* user_process_last_message(void) {
    return user_last_message;
}

static void cleanup_user_memory(void) {
    for (size_t i = 0; i < user_data_count; i++) {
        pmm_free_page(user_data_pages[i]);
    }
    user_data_count = 0;
    if (user_root) {
        vm_free_user_tables(user_root);
        user_root = 0;
    }
    user_table_count = 0;
    user_region_count = 0;
}

static void dbg_hex2(const char* tag, uint64_t v) {
    serial_puts(tag);
    const char* hexd = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hexd[(v >> i) & 0xFULL]);
    }
    serial_puts("\n");
}

int user_process_run(uint64_t entry) {
    uint64_t text_start = (uint64_t)__user_text_start;
    uint64_t text_end = (uint64_t)__user_text_end;
    uint64_t ro_start = (uint64_t)__user_rodata_start;
    uint64_t ro_end = (uint64_t)__user_rodata_end;

    if (user_active || entry < text_start || entry >= text_end) {
        return -1;
    }

    user_table_count = 0;
    user_data_count = 0;
    user_region_count = 0;
    user_brk = USER_HEAP_BASE;
    user_exit_status = 0;
    user_last_status = 0;
    user_last_message = "ok";

    user_root = vm_create_user_root(user_tables, &user_table_count, USER_MAX_TABLES);
    if (!user_root) {
        cleanup_user_memory();
        return -1;
    }

    for (uint64_t va = text_start & ~0xFFFULL; va < text_end; va += 4096ULL) {
        if (!vm_map_page_tracked(user_root, va, va, VM_USER,
                                 user_tables, &user_table_count, USER_MAX_TABLES)) {
            cleanup_user_memory();
            return -1;
        }
    }
    for (uint64_t va = ro_start & ~0xFFFULL; va < ro_end; va += 4096ULL) {
        if (!vm_map_page_tracked(user_root, va, va, VM_USER | VM_NX,
                                 user_tables, &user_table_count, USER_MAX_TABLES)) {
            cleanup_user_memory();
            return -1;
        }
    }

    uint64_t stack_top_page = pmm_alloc_page();
    if (!stack_top_page || !track_data(stack_top_page)) {
        if (stack_top_page) {
            pmm_free_page(stack_top_page);
        }
        cleanup_user_memory();
        return -1;
    }
    if (!vm_map_page_tracked(user_root, USER_STACK_TOP - 4096ULL, stack_top_page,
                             VM_USER | VM_WRITE | VM_NX,
                             user_tables, &user_table_count, USER_MAX_TABLES)) {
        cleanup_user_memory();
        return -1;
    }

    add_region(text_start, text_end - text_start, 1, 0);
    add_region(ro_start, ro_end - ro_start, 1, 0);
    add_region(USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_SIZE, 1, 1);
    add_region(USER_HEAP_BASE, 0, 1, 1);

    uintptr_t api_addr = (USER_STACK_TOP - sizeof(zap_api_t)) & ~15UL;
    /* NOTE: api_addr is the USER-VISIBLE address. We are still on the kernel
     * CR3 here, where that VA identity-maps to unrelated physical memory.
     * Write the struct through the allocated frame's kernel-visible address
     * (identity-mapped PMM frames) so the user mapping actually sees it. */
    uintptr_t api_offset = api_addr - (USER_STACK_TOP - 4096ULL);
    zap_api_t* api = (zap_api_t*)(uintptr_t)(stack_top_page + api_offset);
    api->print = user_api_print;
    api->putc = user_api_putc;
    api->clear = user_api_clear;
    api->getchar = user_api_getchar;
    api->sleep = user_api_sleep;
    api->beep = user_api_beep;
    api->set_cursor = user_api_set_cursor;
    api->reserved = 0;

    dbg_hex2("[USER] frame=", stack_top_page);
    dbg_hex2("[USER] api_va=", api_addr);
    dbg_hex2("[USER] f_print=", (uint64_t)api->print);
    dbg_hex2("[USER] f_putc=", (uint64_t)api->putc);
    dbg_hex2("[USER] f_clear=", (uint64_t)api->clear);
    {
        volatile uint64_t* chk = (volatile uint64_t*)(uintptr_t)(stack_top_page + api_offset);
        dbg_hex2("[USER] chk0=", chk[0]);
    }

    kernel_cr3_saved = vm_current_root();
    user_active = 1;
    uint64_t irq_top = (uint64_t)(uintptr_t)(user_irq_stack + sizeof(user_irq_stack));
    enter_user_process((uint64_t)user_app_trampoline, api_addr, api_addr,
                       user_root, irq_top, entry);

    int status = user_exit_status;
    cleanup_user_memory();
    return status;
}
