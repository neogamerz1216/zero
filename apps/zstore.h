#ifndef ZSTORE_H
#define ZSTORE_H

#include "zap.h"
#include "../drivers/volume.h"

typedef struct {
    zap_header_t header;
    zap_main_func entry;
} store_entry_t;

void zstore_init(void);
int zstore_get_count(void);
const store_entry_t* zstore_get_by_index(int idx);
const store_entry_t* zstore_find(const char* name);
int zstore_install_app(volume_t* vol, const char* name);

#endif
