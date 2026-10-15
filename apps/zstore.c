#include "zstore.h"
#include "../fs/zerofs.h"

extern int app_matrix_main(const zap_api_t* api);
extern int app_tune_main(const zap_api_t* api);
extern int app_sysmon_main(const zap_api_t* api);

#define STORE_APPS_COUNT 3
static store_entry_t catalog[STORE_APPS_COUNT];

static int str_cmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] != '\0' && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void zstore_init(void) {
    catalog[0].header.magic = ZAP_MAGIC;
    str_copy(catalog[0].header.name, "matrix", 20);
    str_copy(catalog[0].header.developer, "NeoZero", 20);
    catalog[0].header.version_major = 1;
    catalog[0].header.version_minor = 2;
    catalog[0].header.payload_size = 512;
    str_copy(catalog[0].header.description, "Digital rain visualizer for terminal", 64);
    catalog[0].entry = app_matrix_main;

    catalog[1].header.magic = ZAP_MAGIC;
    str_copy(catalog[1].header.name, "tune", 20);
    str_copy(catalog[1].header.developer, "SoundPulse", 20);
    catalog[1].header.version_major = 2;
    catalog[1].header.version_minor = 0;
    catalog[1].header.payload_size = 256;
    str_copy(catalog[1].header.description, "8-bit chiptune synthesizer and player", 64);
    catalog[1].entry = app_tune_main;

    catalog[2].header.magic = ZAP_MAGIC;
    str_copy(catalog[2].header.name, "sysmon", 20);
    str_copy(catalog[2].header.developer, "SysTeam", 20);
    catalog[2].header.version_major = 1;
    catalog[2].header.version_minor = 0;
    catalog[2].header.payload_size = 384;
    str_copy(catalog[2].header.description, "Live hardware & memory resource monitor", 64);
    catalog[2].entry = app_sysmon_main;
}

int zstore_get_count(void) {
    return STORE_APPS_COUNT;
}

const store_entry_t* zstore_get_by_index(int idx) {
    if (idx < 0 || idx >= STORE_APPS_COUNT) return 0;
    return &catalog[idx];
}

const store_entry_t* zstore_find(const char* name) {
    for (int i = 0; i < STORE_APPS_COUNT; i++) {
        if (str_cmp(catalog[i].header.name, name) == 0) {
            return &catalog[i];
        }
    }
    return 0;
}

int zstore_install_app(volume_t* vol, const char* name) {
    const store_entry_t* app = zstore_find(name);
    if (!app) return 0;

    char zap_filename[32];
    int j = 0;
    while (app->header.name[j] != '\0') {
        zap_filename[j] = app->header.name[j];
        j++;
    }
    zap_filename[j++] = '.';
    zap_filename[j++] = 'z';
    zap_filename[j++] = 'a';
    zap_filename[j++] = 'p';
    zap_filename[j] = '\0';

    static uint8_t package_buf[sizeof(zap_header_t) + sizeof(uint64_t)];
    zap_header_t* hdr = (zap_header_t*)package_buf;
    *hdr = app->header;

    uint64_t* entry_ptr = (uint64_t*)(package_buf + sizeof(zap_header_t));
    *entry_ptr = (uint64_t)app->entry;

    return zerofs_write(vol, zap_filename, (const char*)package_buf, sizeof(package_buf));
}
