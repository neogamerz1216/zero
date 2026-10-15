#include "mm.h"
#include "../drivers/serial.h"

#define HEAP_SLAB_CLASSES 7
#define HEAP_MAGIC 0x5A48535041504D55ULL
#define HEAP_DIRECT_MAGIC 0x5A48534449524543ULL
#define HEAP_DIRECT_MAX 32

static const size_t heap_sizes[HEAP_SLAB_CLASSES] = { 32, 64, 128, 256, 512, 1024, 2048 };

typedef struct heap_block {
    struct heap_block* next;
} heap_block_t;

typedef struct heap_page {
    uint64_t magic;
    uint64_t cls;
    struct heap_page* next;
    heap_block_t* free;
    uint64_t used;
} heap_page_t;

typedef struct {
    uint64_t magic;
    uint64_t base;
    uint64_t pages;
    int used;
} heap_direct_t;

static heap_page_t* heap_heads[HEAP_SLAB_CLASSES];
static heap_direct_t heap_direct[HEAP_DIRECT_MAX];

static int heap_class_for(size_t size) {
    for (int i = 0; i < HEAP_SLAB_CLASSES; i++) {
        if (size <= heap_sizes[i]) {
            return i;
        }
    }
    return -1;
}

static void heap_fill(void* ptr, size_t size, uint8_t pattern) {
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = pattern;
    }
}

void* kmalloc(size_t size) {
    if (size == 0) {
        return 0;
    }

    int cls = heap_class_for(size);
    if (cls < 0) {
        uint64_t pages = (size + PMM_FRAME_SIZE - 1ULL) / PMM_FRAME_SIZE;
        uint64_t base = pmm_alloc_pages(pages);
        if (!base) {
            return 0;
        }
        for (int i = 0; i < HEAP_DIRECT_MAX; i++) {
            if (!heap_direct[i].used) {
                heap_direct[i].magic = HEAP_DIRECT_MAGIC;
                heap_direct[i].base = base;
                heap_direct[i].pages = pages;
                heap_direct[i].used = 1;
                return (void*)(uintptr_t)base;
            }
        }
        pmm_free_pages(base, pages);
        return 0;
    }

    heap_page_t* page = heap_heads[cls];
    if (!page || !page->free) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            return 0;
        }
        page = (heap_page_t*)(uintptr_t)phys;
        page->magic = HEAP_MAGIC;
        page->cls = (uint64_t)cls;
        page->next = heap_heads[cls];
        page->free = 0;
        page->used = 0;

        size_t off = (sizeof(heap_page_t) + 15) & ~(size_t)15;
        size_t chunk = heap_sizes[cls];
        while (off + chunk <= PMM_FRAME_SIZE) {
            heap_block_t* block = (heap_block_t*)((uint8_t*)page + off);
            block->next = page->free;
            page->free = block;
            off += chunk;
        }
        if (!page->free) {
            pmm_free_page(phys);
            return 0;
        }
        heap_heads[cls] = page;
    }

    heap_block_t* block = page->free;
    page->free = block->next;
    page->used++;
    heap_fill(block, heap_sizes[cls], 0);
    return block;
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }
    uintptr_t addr = (uintptr_t)ptr;

    if ((addr & (PMM_FRAME_SIZE - 1ULL)) == 0) {
        for (int i = 0; i < HEAP_DIRECT_MAX; i++) {
            if (heap_direct[i].used && heap_direct[i].base == addr) {
                heap_direct[i].used = 0;
                pmm_free_pages(heap_direct[i].base, heap_direct[i].pages);
                heap_direct[i].base = 0;
                heap_direct[i].pages = 0;
                return;
            }
        }
    }

    uintptr_t base = addr & ~(uintptr_t)(PMM_FRAME_SIZE - 1ULL);
    if (base < PMM_RESERVE_BELOW || base >= PMM_MANAGED_BYTES) {
        return;
    }
    heap_page_t* page = (heap_page_t*)base;
    if (page->magic != HEAP_MAGIC || page->cls >= (uint64_t)HEAP_SLAB_CLASSES) {
        return;
    }
    uintptr_t limit = base + PMM_FRAME_SIZE;
    if (addr < base + sizeof(heap_page_t) || addr >= limit) {
        return;
    }

    heap_block_t* block = (heap_block_t*)ptr;
    block->next = page->free;
    page->free = block;
    if (page->used > 0) {
        page->used--;
    }
    if (page->used == 0) {
        heap_page_t** link = &heap_heads[page->cls];
        while (*link && *link != page) {
            link = &(*link)->next;
        }
        if (*link) {
            *link = page->next;
        }
        pmm_free_page(base);
    }
}

int heap_selftest(void) {
    for (int i = 0; i < HEAP_SLAB_CLASSES; i++) {
        void* p = kmalloc(heap_sizes[i]);
        if (!p) {
            return 0;
        }
        heap_fill(p, heap_sizes[i], (uint8_t)(0xA0 + i));
        kfree(p);
    }

    void* a = kmalloc(64);
    void* b = kmalloc(64);
    void* c = kmalloc(64);
    void* big = kmalloc(4096);
    if (!a || !b || !c || !big) {
        return 0;
    }
    kfree(b);
    void* d = kmalloc(64);
    if (!d) {
        return 0;
    }
    kfree(a);
    kfree(c);
    kfree(d);
    kfree(big);
    return 1;
}

static void dbg_dec(const char* tag, uint64_t v) {
    serial_puts(tag);
    char buf[24];
    int i = 0;
    if (v == 0) {
        serial_puts("0\n");
        return;
    }
    while (v > 0 && i < 20) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        serial_putc(buf[j]);
    }
    serial_puts("\n");
}

int mm_selftest(void) {
    uint64_t before = pmm_free_bytes();
    dbg_dec("[MM] free before=", before);
    dbg_dec("[MM] total=", pmm_total_bytes());
    uint64_t p1 = pmm_alloc_page();
    uint64_t p2 = pmm_alloc_pages(2);
    dbg_dec("[MM] p1=", p1);
    dbg_dec("[MM] p2=", p2);
    if (!p1 || !p2) {
        return 0;
    }
    if (pmm_free_bytes() + 3 * PMM_FRAME_SIZE != before) {
        pmm_free_page(p1);
        pmm_free_pages(p2, 2);
        return 0;
    }
    pmm_free_page(p1);
    pmm_free_pages(p2, 2);
    if (pmm_free_bytes() != before) {
        return 0;
    }
    return heap_selftest();
}
