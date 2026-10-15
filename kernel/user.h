#ifndef ZEROOS_USER_H
#define ZEROOS_USER_H

#include <stdint.h>
#include <stddef.h>

int user_process_active(void);
int user_memory_readable(uint64_t addr, size_t len);
int user_copy_from_user(void* dst, uint64_t src, size_t len);
long user_process_set_brk(long new_brk);
void user_process_exit(int status) __attribute__((noreturn));
int user_process_run(uint64_t entry);
unsigned long user_process_last_status(void);
const char* user_process_last_message(void);
void user_handle_fatal(uint64_t num, uint64_t err, uint64_t addr);
int user_handle_page_fault(uint64_t addr, uint64_t err);
void user_return_to_kernel(void) __attribute__((noreturn));

#endif
