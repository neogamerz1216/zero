#ifndef DRIVMAN_H
#define DRIVMAN_H

#include <stdint.h>

#define DRIVMAN_MAX_DRIVERS 32

typedef int (*drv_init_fn)(void);

typedef struct {
    char name[32];
    drv_init_fn init;
    int installed;
    int initialized;
} drv_entry_t;

void drvman_init(void);
int drvman_register(const char* name, drv_init_fn init);
int drvman_load(const char* name);
int drvman_init_all(void);
void drvman_list(void);
int drvman_count(void);

#endif
