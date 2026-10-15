#ifndef ZEROOS_VM_H
#define ZEROOS_VM_H

#include <stdint.h>
#include <stddef.h>

#define VM_PRESENT (1ULL << 0)
#define VM_WRITE (1ULL << 1)
#define VM_USER (1ULL << 2)
#define VM_NX (1ULL << 63)

uint64_t vm_current_root(void);
void vm_write_root(uint64_t root);
void vm_invalidate_page(uint64_t virt);
uint64_t vm_create_user_root(uint64_t* tables, size_t* table_count, size_t table_max);
int vm_page_present(uint64_t root, uint64_t virt);
int vm_map_page_tracked(uint64_t root, uint64_t virt, uint64_t phys, uint64_t flags,
                         uint64_t* tables, size_t* table_count, size_t table_max);
void vm_free_user_tables(uint64_t root);

#endif
