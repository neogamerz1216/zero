#ifndef ZEROFS_H
#define ZEROFS_H

#include <stdint.h>
#include "../drivers/volume.h"

#define ZEROFS_MAGIC         0x5A45524F
#define ZEROFS_START_SECTOR  100
#define ZEROFS_MAX_FILES     32
#define ZEROFS_MAX_FILENAME  24
#define ZEROFS_DIRECT_BLOCKS 8
#define ZEROFS_BLOCK_SIZE    512

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t inode_count;
    uint32_t free_blocks;
    char label[16];
} __attribute__((packed)) zerofs_superblock_t;

typedef struct {
    char name[ZEROFS_MAX_FILENAME];
    uint32_t size_bytes;
    uint32_t used;
    uint32_t blocks[ZEROFS_DIRECT_BLOCKS];
    uint8_t day;
    uint8_t month;
    uint32_t year;
} __attribute__((packed)) zerofs_inode_t;

void zerofs_init(volume_t* vol);
int zerofs_format(volume_t* vol, const char* label);
int zerofs_create(volume_t* vol, const char* filename);
int zerofs_write(volume_t* vol, const char* filename, const char* data, uint32_t len);
int zerofs_read(volume_t* vol, const char* filename, char* buffer, uint32_t max_len);
int zerofs_delete(volume_t* vol, const char* filename);
void zerofs_list(volume_t* vol);

#endif
