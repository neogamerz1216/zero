#include "process.h"

#define MAX_PROCESSES 4
#define STACK_SIZE    8192

typedef struct {
    uint64_t rsp;
    int active;
    uint8_t stack[STACK_SIZE];
} pcb_t;

static pcb_t process_table[MAX_PROCESSES];
static int current_pid = 0;

extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp);

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].active = 0;
        process_table[i].rsp = 0;
    }
    process_table[0].active = 1;
    current_pid = 0;
}

int process_create(void (*entry_point)(void)) {
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (!process_table[i].active) {
            process_table[i].active = 1;

            uint64_t stack_top_addr = ((uint64_t)&process_table[i].stack[STACK_SIZE]) & ~0xFULL;
            uint64_t* stack_top = (uint64_t*)stack_top_addr;

            *(--stack_top) = 0;
            *(--stack_top) = (uint64_t)entry_point;

            for (int j = 0; j < 6; j++) {
                *(--stack_top) = 0;
            }

            process_table[i].rsp = (uint64_t)stack_top;
            return i;
        }
    }
    return -1;
}

void process_schedule(void) {
    int next_pid = current_pid;
    int attempts = 0;

    do {
        next_pid = (next_pid + 1) % MAX_PROCESSES;
        attempts++;
    } while (!process_table[next_pid].active && attempts < MAX_PROCESSES);

    if (next_pid == current_pid || !process_table[next_pid].active) {
        return;
    }

    int prev_pid = current_pid;
    current_pid = next_pid;

    context_switch(&process_table[prev_pid].rsp, process_table[next_pid].rsp);
}
