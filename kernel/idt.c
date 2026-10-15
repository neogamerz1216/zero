#include "idt.h"
#include "user.h"
#include "../drivers/serial.h"

struct idt_entry_64 {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr_64 {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry_64 idt[256];
static struct idt_ptr_64 idtr;

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void isr32_timer(void);
extern void isr33_keyboard(void);

static inline uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(v));
    return v;
}

static void serial_hex(uint64_t v) {
    const char* hex = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        char c = hex[(v >> i) & 0xFULL];
        serial_putc(c);
    }
}

static void dump_user_regs(uint64_t* f) {
    static const char* names[15] = {
        "r15=", "r14=", "r13=", "r12=", "r11=", "r10=", "r9=",
        "r8=", "rbp=", "rdi=", "rsi=", "rdx=", "rcx=", "rbx=", "rax="
    };
    for (int i = 0; i < 15; i++) {
        serial_puts(names[i]);
        serial_hex(f[i]);
        serial_puts(i % 3 == 2 ? "\n" : " ");
    }
    serial_puts("rip=");
    serial_hex(f[17]);
    serial_puts(" rsp=");
    serial_hex(f[20]);
    serial_puts("\n");
}

uint64_t exception_dispatch(uint64_t num, uint64_t err, uint64_t* frame) {
    if (user_process_active() && frame) {
        uint64_t cs = frame[18];
        if ((cs & 3ULL) == 3ULL) {
            if (num == 14) {
                uint64_t addr = read_cr2();
                serial_puts("[ZeroOS] user #PF err=");
                serial_hex(err);
                serial_puts(" addr=");
                serial_hex(addr);
                serial_puts("\n");
                dump_user_regs(frame);
                if (user_handle_page_fault(addr, err)) {
                    return 0;
                }
                user_handle_fatal(num, err, addr);
            } else {
                serial_puts("[ZeroOS] user fault vector=");
                serial_hex(num);
                serial_puts(" err=");
                serial_hex(err);
                serial_puts("\n");
                dump_user_regs(frame);
                user_handle_fatal(num, err, frame[17]);
            }
        }
    }

    serial_puts("\n[ZeroOS] Kernel fault halting. vector=");
    serial_hex(num);
    serial_puts(" err=");
    serial_hex(err);
    if (num == 14) {
        serial_puts(" cr2=");
        serial_hex(read_cr2());
    }
    if (frame) {
        serial_puts(" rip=");
        serial_hex(frame[17]);
        serial_puts(" cs=");
        serial_hex(frame[18]);
        serial_puts(" rflags=");
        serial_hex(frame[19]);
    }
    serial_puts("\n");

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

void idt_set_gate(uint8_t num, uint64_t handler, uint8_t dpl) {
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].selector    = 0x08;
    idt[num].ist         = 0;
    idt[num].type_attr   = (uint8_t)(0x8E | ((dpl & 3) << 5));
    idt[num].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].zero        = 0;
}

void idt_init(void) {
    void (*isrs[32])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        idt_set_gate((uint8_t)i, (uint64_t)isrs[i], 3);
    }

    idt_set_gate(32, (uint64_t)isr32_timer, 0);
    idt_set_gate(33, (uint64_t)isr33_keyboard, 0);

    idtr.base = (uint64_t)&idt[0];
    idtr.limit = (uint16_t)(sizeof(idt) - 1);

    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
