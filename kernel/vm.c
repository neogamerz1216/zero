#include "vm.h"
#include "mm.h"

#define VM_TABLE_MASK 0x000FFFFFFFFFF000ULL
#define VM_LARGE_PAGE_MASK (~(uint64_t)0x1FFFFFULL)
#define VM_ENTRIES 512

static inline uint64_t vm_read_cr3(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void vm_write_cr3_raw(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(v) : "memory");
}

uint64_t vm_current_root(void) {
    return vm_read_cr3() & VM_TABLE_MASK;
}

void vm_write_root(uint64_t root) {
    vm_write_cr3_raw(root);
}

void vm_invalidate_page(uint64_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static int vm_track(uint64_t phys, uint64_t* tables, size_t* count, size_t max) {
    if (*count >= max) {
        return 0;
    }
    tables[*count] = phys;
    (*count)++;
    return 1;
}

static uint64_t vm_get_table(uint64_t parent, uint64_t index, int create,
                             uint64_t* tables, size_t* count, size_t max) {
    uint64_t* p = (uint64_t*)(uintptr_t)parent;
    uint64_t e = p[index];
    if (e & VM_PRESENT) {
        return e & VM_TABLE_MASK;
    }
    if (!create) {
        return 0;
    }
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
        return 0;
    }
    if (!vm_track(phys, tables, count, max)) {
        pmm_free_page(phys);
        return 0;
    }
    p[index] = phys | VM_PRESENT | VM_WRITE | VM_USER;
    return phys;
}

uint64_t vm_create_user_root(uint64_t* tables, size_t* table_count, size_t table_max) {
    uint64_t boot_root = vm_current_root();
    uint64_t* boot_pml4 = (uint64_t*)(uintptr_t)boot_root;
    if (!(boot_pml4[0] & VM_PRESENT)) {
        return 0;
    }
    uint64_t* boot_pdpt = (uint64_t*)(uintptr_t)(boot_pml4[0] & VM_TABLE_MASK);
    if (!(boot_pdpt[0] & VM_PRESENT)) {
        return 0;
    }
    uint64_t* boot_pd = (uint64_t*)(uintptr_t)(boot_pdpt[0] & VM_TABLE_MASK);

    uint64_t root = pmm_alloc_page();
    uint64_t pdpt = pmm_alloc_page();
    uint64_t pd = pmm_alloc_page();
    if (!root || !pdpt || !pd) {
        if (root) pmm_free_page(root);
        if (pdpt) pmm_free_page(pdpt);
        if (pd) pmm_free_page(pd);
        return 0;
    }
    *table_count = 0;
    if (!vm_track(root, tables, table_count, table_max) ||
        !vm_track(pdpt, tables, table_count, table_max) ||
        !vm_track(pd, tables, table_count, table_max)) {
        pmm_free_page(root);
        pmm_free_page(pdpt);
        pmm_free_page(pd);
        *table_count = 0;
        return 0;
    }

    uint64_t* pml4 = (uint64_t*)(uintptr_t)root;
    uint64_t* new_pdpt = (uint64_t*)(uintptr_t)pdpt;
    uint64_t* new_pd = (uint64_t*)(uintptr_t)pd;
    for (int i = 1; i < (int)VM_ENTRIES; i++) {
        pml4[i] = boot_pml4[i];
    }
    pml4[0] = pdpt | VM_PRESENT | VM_WRITE | VM_USER;
    new_pdpt[0] = pd | VM_PRESENT | VM_WRITE | VM_USER;
    for (int i = 1; i < (int)VM_ENTRIES; i++) {
        new_pdpt[i] = 0;
    }
    for (int i = 0; i < (int)VM_ENTRIES; i++) {
        uint64_t e = boot_pd[i];
        if (!(e & VM_PRESENT)) {
            new_pd[i] = 0;
        } else {
            new_pd[i] = e & ~VM_USER;
        }
    }
    return root;
}

int vm_page_present(uint64_t root, uint64_t virt) {
    if (!root || (virt & 0xFFFULL)) {
        return 0;
    }
    uint64_t* pml4 = (uint64_t*)(uintptr_t)root;
    uint64_t e = pml4[(virt >> 39) & 0x1FFULL];
    if (!(e & VM_PRESENT)) {
        return 0;
    }
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(e & VM_TABLE_MASK);
    e = pdpt[(virt >> 30) & 0x1FFULL];
    if (!(e & VM_PRESENT)) {
        return 0;
    }
    uint64_t* pd = (uint64_t*)(uintptr_t)(e & VM_TABLE_MASK);
    e = pd[(virt >> 21) & 0x1FFULL];
    if (!(e & VM_PRESENT)) {
        return 0;
    }
    if (e & (1ULL << 7)) {
        return 1;
    }
    uint64_t* pt = (uint64_t*)(uintptr_t)(e & VM_TABLE_MASK);
    return (pt[(virt >> 12) & 0x1FFULL] & VM_PRESENT) != 0;
}

int vm_map_page_tracked(uint64_t root, uint64_t virt, uint64_t phys, uint64_t flags,
                         uint64_t* tables, size_t* table_count, size_t table_max) {
    if ((virt & 0xFFFULL) || (phys & 0xFFFULL) || !root) {
        return 0;
    }
    uint64_t pml4i = (virt >> 39) & 0x1FFULL;
    uint64_t pdpti = (virt >> 30) & 0x1FFULL;
    uint64_t pdi = (virt >> 21) & 0x1FFULL;
    uint64_t pti = (virt >> 12) & 0x1FFULL;

    uint64_t pdpt = vm_get_table(root, pml4i, 1, tables, table_count, table_max);
    uint64_t pd = vm_get_table(pdpt, pdpti, 1, tables, table_count, table_max);
    if (!pdpt || !pd) {
        return 0;
    }
    uint64_t* pdp = (uint64_t*)(uintptr_t)pd;
    if (pdp[pdi] & VM_PRESENT && (pdp[pdi] & (1ULL << 7))) {
        uint64_t base = pdp[pdi] & VM_LARGE_PAGE_MASK;
        uint64_t prot = pdp[pdi] & (VM_PRESENT | VM_WRITE);
        uint64_t pt = pmm_alloc_page();
        if (!pt) {
            return 0;
        }
        if (!vm_track(pt, tables, table_count, table_max)) {
            pmm_free_page(pt);
            return 0;
        }
        uint64_t* pte = (uint64_t*)(uintptr_t)pt;
        for (int i = 0; i < (int)VM_ENTRIES; i++) {
            pte[i] = (base + (uint64_t)i * 4096ULL) | prot;
        }
        pdp[pdi] = pt | VM_PRESENT | VM_WRITE | VM_USER;
    }
    uint64_t pt = vm_get_table(pd, pdi, 1, tables, table_count, table_max);
    if (!pt) {
        return 0;
    }
    uint64_t* pte = (uint64_t*)(uintptr_t)pt;
    pte[pti] = (phys & VM_TABLE_MASK) | VM_PRESENT | (flags & (VM_WRITE | VM_USER | VM_NX));
    vm_invalidate_page(virt);
    return 1;
}

void vm_free_user_tables(uint64_t root) {
    if (!root) {
        return;
    }
    uint64_t* pml4 = (uint64_t*)(uintptr_t)root;
    if (pml4[0] & VM_PRESENT) {
        uint64_t* pdpt = (uint64_t*)(uintptr_t)(pml4[0] & VM_TABLE_MASK);
        if (pdpt[0] & VM_PRESENT) {
            uint64_t* pd = (uint64_t*)(uintptr_t)(pdpt[0] & VM_TABLE_MASK);
            for (int i = 0; i < (int)VM_ENTRIES; i++) {
                if ((pd[i] & VM_PRESENT) && !(pd[i] & (1ULL << 7))) {
                    pmm_free_page(pd[i] & VM_TABLE_MASK);
                    pd[i] = 0;
                }
            }
            pmm_free_page(pdpt[0] & VM_TABLE_MASK);
            pdpt[0] = 0;
        }
        pmm_free_page(pml4[0] & VM_TABLE_MASK);
        pml4[0] = 0;
    }
    pmm_free_page(root);
}
