#include "zerofs.h"
#include "../drivers/rtc.h"
#include "../drivers/vga.h"

static int str_cmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static void str_cpy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] != '\0' && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void zerofs_init(volume_t* vol) {
    if (!vol || !vol->is_online) return;

    static uint8_t buf[512];
    volume_read(vol, ZEROFS_START_SECTOR, buf);
    zerofs_superblock_t* sb = (zerofs_superblock_t*)buf;

    if (sb->magic != ZEROFS_MAGIC) {
        zerofs_format(vol, "ZeroDrive");
    }
}

int zerofs_format(volume_t* vol, const char* label) {
    if (!vol || !vol->is_online) return 0;

    static uint8_t sector_zero[512];
    for (int i = 0; i < 512; i++) sector_zero[i] = 0;

    zerofs_superblock_t* sb = (zerofs_superblock_t*)sector_zero;
    sb->magic = ZEROFS_MAGIC;
    sb->total_blocks = 1000;
    sb->inode_count = ZEROFS_MAX_FILES;
    sb->free_blocks = 950;
    str_cpy(sb->label, label, 16);
    volume_write(vol, ZEROFS_START_SECTOR, sector_zero);

    for (int i = 0; i < 512; i++) sector_zero[i] = 0;
    for (int s = 1; s <= 8; s++) {
        volume_write(vol, ZEROFS_START_SECTOR + s, sector_zero);
    }
    return 1;
}

static int read_inode_table(volume_t* vol, zerofs_inode_t* inodes) {
    uint8_t* target = (uint8_t*)inodes;
    for (int s = 1; s <= 4; s++) {
        if (!volume_read(vol, ZEROFS_START_SECTOR + s, target + ((s - 1) * 512))) {
            return 0;
        }
    }
    return 1;
}

static int write_inode_table(volume_t* vol, zerofs_inode_t* inodes) {
    uint8_t* src = (uint8_t*)inodes;
    for (int s = 1; s <= 4; s++) {
        if (!volume_write(vol, ZEROFS_START_SECTOR + s, src + ((s - 1) * 512))) {
            return 0;
        }
    }
    return 1;
}

int zerofs_create(volume_t* vol, const char* filename) {
    if (!vol || !vol->is_online) return 0;

    static zerofs_inode_t inodes[ZEROFS_MAX_FILES];
    if (!read_inode_table(vol, inodes)) return 0;

    for (int i = 0; i < ZEROFS_MAX_FILES; i++) {
        if (inodes[i].used && str_cmp(inodes[i].name, filename) == 0) {
            return 0;
        }
    }

    for (int i = 0; i < ZEROFS_MAX_FILES; i++) {
        if (!inodes[i].used) {
            inodes[i].used = 1;
            inodes[i].size_bytes = 0;
            str_cpy(inodes[i].name, filename, ZEROFS_MAX_FILENAME);

            for (int b = 0; b < ZEROFS_DIRECT_BLOCKS; b++) {
                inodes[i].blocks[b] = ZEROFS_START_SECTOR + 20 + (i * ZEROFS_DIRECT_BLOCKS) + b;
            }

            rtc_time_t t = rtc_get_time();
            inodes[i].day = t.day;
            inodes[i].month = t.month;
            inodes[i].year = t.year;

            return write_inode_table(vol, inodes);
        }
    }
    return 0;
}

int zerofs_write(volume_t* vol, const char* filename, const char* data, uint32_t len) {
    if (!vol || !vol->is_online) return 0;

    static zerofs_inode_t inodes[ZEROFS_MAX_FILES];
    if (!read_inode_table(vol, inodes)) return 0;

    int idx = -1;
    for (int i = 0; i < ZEROFS_MAX_FILES; i++) {
        if (inodes[i].used && str_cmp(inodes[i].name, filename) == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        if (!zerofs_create(vol, filename)) return 0;
        read_inode_table(vol, inodes);
        for (int i = 0; i < ZEROFS_MAX_FILES; i++) {
            if (inodes[i].used && str_cmp(inodes[i].name, filename) == 0) {
                idx = i;
                break;
            }
        }
    }

    inodes[idx].size_bytes = len;
    write_inode_table(vol, inodes);

    static uint8_t block_buf[512];
    uint32_t written = 0;
    int blk = 0;

    while (written < len && blk < ZEROFS_DIRECT_BLOCKS) {
        for (int b = 0; b < 512; b++) {
            if (written < len) block_buf[b] = data[written++];
            else block_buf[b] = 0;
        }
        volume_write(vol, inodes[idx].blocks[blk], block_buf);
        blk++;
    }

    return 1;
}

int zerofs_read(volume_t* vol, const char* filename, char* buffer, uint32_t max_len) {
    if (!vol || !vol->is_online) return 0;

    static zerofs_inode_t inodes[ZEROFS_MAX_FILES];
    if (!read_inode_table(vol, inodes)) return 0;

    for (int i = 0; i < ZEROFS_MAX_FILES; i++) {
        if (inodes[i].used && str_cmp(inodes[i].name, filename) == 0) {
            uint32_t bytes_to_read = inodes[i].size_bytes;
            if (bytes_to_read > max_len - 1) bytes_to_read = max_len - 1;

            static uint8_t block_buf[512];
            uint32_t read_bytes = 0;
            int blk = 0;

            while (read_bytes < bytes_to_read && blk < ZEROFS_DIRECT_BLOCKS) {
                volume_read(vol, inodes[i].blocks[blk], block_buf);
                for (int b = 0; b < 512 && read_bytes < bytes_to_read; b++) {
                    buffer[read_bytes++] = (char)block_buf[b];
                }
                blk++;
            }
            buffer[read_bytes] = '\0';
            return read_bytes;
        }
    }
    return -1;
}

int zerofs_delete(volume_t* vol, const char* filename) {
    if (!vol || !vol->is_online) return 0;

    static zerofs_inode_t inodes[ZEROFS_MAX_FILES];
    if (!read_inode_table(vol, inodes)) return 0;

    for (int i = 0; i < ZEROFS_MAX_FILES; i++) {
        if (inodes[i].used && str_cmp(inodes[i].name, filename) == 0) {
            inodes[i].used = 0;
            inodes[i].size_bytes = 0;
            inodes[i].name[0] = '\0';
            return write_inode_table(vol, inodes);
        }
    }
    return 0;
}

void zerofs_list(volume_t* vol) {
    if (!vol || !vol->is_online) {
        vga_puts("Target drive offline.\n", COLOR_LIGHT_RED);
        return;
    }

    static zerofs_inode_t inodes[ZEROFS_MAX_FILES];
    if (!read_inode_table(vol, inodes)) {
        vga_puts("Filesystem read fault!\n", COLOR_LIGHT_RED);
        return;
    }

    vga_puts("\n[ Volume Directory: ", COLOR_LIGHT_CYAN);
    vga_puts(vol->name, COLOR_LIGHT_GREEN);
    vga_puts(" ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  FILENAME               SIZE (BYTES)    CREATED\n", COLOR_DARK_GREY);
    vga_puts("  --------------------   ------------    ----------\n", COLOR_DARK_GREY);

    int count = 0;
    for (int i = 0; i < ZEROFS_MAX_FILES; i++) {
        if (inodes[i].used) {
            count++;
            vga_puts("  ", COLOR_WHITE);
            vga_puts(inodes[i].name, COLOR_LIGHT_GREEN);

            int name_len = 0;
            while (inodes[i].name[name_len] != '\0') name_len++;
            int pad = 23 - name_len;
            for (int p = 0; p < pad; p++) vga_putc(' ', COLOR_WHITE);

            uint32_t s = inodes[i].size_bytes;
            char num_buf[16]; int n_idx = 0;
            if (s == 0) num_buf[n_idx++] = '0';
            while (s > 0) { num_buf[n_idx++] = '0' + (s % 10); s /= 10; }
            for (int r = n_idx - 1; r >= 0; r--) vga_putc(num_buf[r], COLOR_WHITE);
            for (int p = 0; p < 16 - n_idx; p++) vga_putc(' ', COLOR_WHITE);

            vga_putc('0' + (inodes[i].day / 10), COLOR_LIGHT_CYAN);
            vga_putc('0' + (inodes[i].day % 10), COLOR_LIGHT_CYAN);
            vga_putc('/', COLOR_LIGHT_CYAN);
            vga_putc('0' + (inodes[i].month / 10), COLOR_LIGHT_CYAN);
            vga_putc('0' + (inodes[i].month % 10), COLOR_LIGHT_CYAN);
            vga_putc('/', COLOR_LIGHT_CYAN);
            vga_puts("2026\n", COLOR_LIGHT_CYAN);
        }
    }

    if (count == 0) {
        vga_puts("  (No files found on this volume)\n", COLOR_DARK_GREY);
    }
    vga_puts("\n", COLOR_WHITE);
}
