#include "mm.h"

#define PMM_MAX_FRAMES (1024ULL * 1024ULL)
#define PMM_MANAGED_FRAMES (PMM_MANAGED_BYTES / PMM_FRAME_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8ULL)

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} __attribute__((packed)) e820_entry_t;

static uint8_t pmm_bitmap[PMM_BITMAP_BYTES];
static uint64_t pmm_total_frames = 0;
static uint64_t pmm_free_frames = 0;
static uint64_t pmm_hint = 0;

static inline void pmm_set_used(uint64_t frame) {
    pmm_bitmap[frame >> 3] |= (uint8_t)(1u << (frame & 7));
}

static inline void pmm_set_free(uint64_t frame) {
    pmm_bitmap[frame >> 3] &= (uint8_t)~(1u << (frame & 7));
}

static inline int pmm_is_free(uint64_t frame) {
    return (pmm_bitmap[frame >> 3] & (1u << (frame & 7))) == 0;
}

static void pmm_zero_page(uint64_t phys) {
    volatile uint64_t* p = (volatile uint64_t*)(uintptr_t)phys;
    for (uint64_t i = 0; i < PMM_FRAME_SIZE / 8ULL; i++) {
        p[i] = 0;
    }
}

static void pmm_mark_range(uint64_t start, uint64_t end, int available) {
    if (end <= start) {
        return;
    }
    if (start >= PMM_MANAGED_BYTES) {
        return;
    }
    if (end > PMM_MANAGED_BYTES) {
        end = PMM_MANAGED_BYTES;
    }

    uint64_t first = start / PMM_FRAME_SIZE;
    uint64_t last = end / PMM_FRAME_SIZE;
    if (last > PMM_MANAGED_FRAMES) {
        last = PMM_MANAGED_FRAMES;
    }
    for (uint64_t f = first; f < last; f++) {
        if (available) {
            pmm_set_free(f);
        } else {
            pmm_set_used(f);
        }
    }
}

static void pmm_mark_usable(uint64_t start, uint64_t end) {
    if (end <= start) {
        return;
    }
    if (start < PMM_RESERVE_BELOW) {
        start = PMM_RESERVE_BELOW;
    }
    if (end <= start) {
        return;
    }

    if (start < RAMDISK_END && end > RAMDISK_BEGIN) {
        uint64_t before_end = start < RAMDISK_BEGIN ? RAMDISK_BEGIN : start;
        pmm_mark_range(start, before_end, 1);
        uint64_t after_start = end > RAMDISK_END ? RAMDISK_END : end;
        pmm_mark_range(after_start, end, 1);
        return;
    }
    pmm_mark_range(start, end, 1);
}

void pmm_init(void) {
    for (uint64_t i = 0; i < PMM_BITMAP_BYTES; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    uint32_t count = *(volatile uint32_t*)0x7000;
    if (count > 128) {
        count = 128;
    }
    volatile e820_entry_t* map = (volatile e820_entry_t*)0x5000;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t base = map[i].base;
        uint64_t length = map[i].length;
        uint32_t type = map[i].type;
        if (type != 1 || length == 0) {
            continue;
        }
        uint64_t start = (base + PMM_FRAME_SIZE - 1ULL) & ~(PMM_FRAME_SIZE - 1ULL);
        uint64_t end = (base + length) & ~(PMM_FRAME_SIZE - 1ULL);
        pmm_mark_usable(start, end);
    }

    pmm_total_frames = 0;
    pmm_free_frames = 0;
    for (uint64_t f = 0; f < PMM_MANAGED_FRAMES; f++) {
        if (pmm_is_free(f)) {
            pmm_total_frames++;
            pmm_free_frames++;
        }
    }
    pmm_hint = PMM_RESERVE_BELOW / PMM_FRAME_SIZE;
}

uint64_t pmm_alloc_page(void) {
    for (uint64_t n = 0; n < PMM_MANAGED_FRAMES; n++) {
        uint64_t f = pmm_hint + n;
        if (f >= PMM_MANAGED_FRAMES) {
            f -= PMM_MANAGED_FRAMES;
        }
        if (pmm_is_free(f)) {
            pmm_set_used(f);
            pmm_free_frames--;
            pmm_hint = f + 1;
            if (pmm_hint >= PMM_MANAGED_FRAMES) {
                pmm_hint = 0;
            }
            uint64_t phys = f * PMM_FRAME_SIZE;
            pmm_zero_page(phys);
            return phys;
        }
    }
    return 0;
}

uint64_t pmm_alloc_pages(uint64_t count) {
    if (count == 0 || count > PMM_MANAGED_FRAMES) {
        return 0;
    }
    for (uint64_t f = 0; f + count <= PMM_MANAGED_FRAMES; f++) {
        uint64_t ok = 1;
        for (uint64_t i = 0; i < count; i++) {
            if (!pmm_is_free(f + i)) {
                ok = 0;
                f += i;
                break;
            }
        }
        if (ok) {
            for (uint64_t i = 0; i < count; i++) {
                pmm_set_used(f + i);
                pmm_zero_page((f + i) * PMM_FRAME_SIZE);
            }
            pmm_free_frames -= count;
            return f * PMM_FRAME_SIZE;
        }
    }
    return 0;
}

static int pmm_valid_managed(uint64_t phys) {
    return phys != 0 &&
           (phys & (PMM_FRAME_SIZE - 1ULL)) == 0 &&
           phys >= PMM_RESERVE_BELOW &&
           phys < PMM_MANAGED_BYTES &&
           (phys < RAMDISK_BEGIN || phys >= RAMDISK_END);
}

void pmm_free_page(uint64_t phys) {
    if (!pmm_valid_managed(phys)) {
        return;
    }
    uint64_t f = phys / PMM_FRAME_SIZE;
    if (pmm_is_free(f)) {
        return;
    }
    pmm_set_free(f);
    pmm_free_frames++;
}

void pmm_free_pages(uint64_t phys, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        pmm_free_page(phys + i * PMM_FRAME_SIZE);
    }
}

uint64_t pmm_total_bytes(void) {
    return pmm_total_frames * PMM_FRAME_SIZE;
}

uint64_t pmm_free_bytes(void) {
    return pmm_free_frames * PMM_FRAME_SIZE;
}
