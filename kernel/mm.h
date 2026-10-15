#ifndef ZEROOS_MM_H
#define ZEROOS_MM_H

#include <stdint.h>
#include <stddef.h>

#define PMM_FRAME_SIZE 4096ULL
#define PMM_MANAGED_BYTES 0x40000000ULL
#define PMM_RESERVE_BELOW 0x200000ULL
#define RAMDISK_BEGIN 0x400000ULL
#define RAMDISK_END 0x700000ULL

void pmm_init(void);
uint64_t pmm_alloc_page(void);
uint64_t pmm_alloc_pages(uint64_t count);
void pmm_free_page(uint64_t phys);
void pmm_free_pages(uint64_t phys, uint64_t count);
uint64_t pmm_total_bytes(void);
uint64_t pmm_free_bytes(void);

void* kmalloc(size_t size);
void kfree(void* ptr);
int heap_selftest(void);
int mm_selftest(void);

#endif
