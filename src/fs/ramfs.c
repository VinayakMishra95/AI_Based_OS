#include "fs.h"
#include "ata.h"
#include "string.h"

#define FS_LBA_START 200
#define FS_MAGIC     0x41455448u /* "AETH" */

typedef struct {
    uint32_t magic;
    uint32_t version;
    ramfs_file_t files[RAMFS_MAX_FILES];
} __attribute__((packed)) disk_fs_image_t;

static disk_fs_image_t fs_img;
static bool persisted_on_disk = false;

static int find_file(const char* name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (fs_img.files[i].used && strcmp(fs_img.files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void ramfs_load_defaults(void) {
    memset(&fs_img, 0, sizeof(fs_img));
    fs_img.magic = FS_MAGIC;
    fs_img.version = 1;

    ramfs_create("README.txt");
    ramfs_write("README.txt",
        "AetherOS v0.2 - Persistent ATA Disk + AI Action Shell\n"
        "Run 'sync' to save files to disk across reboots.");

    ramfs_create("system.cfg");
    ramfs_write("system.cfg",
        "kernel=aether-0.2\narch=i386\ndisplay=vbe-32bpp\ndisk=ata-pio-lba200");
}

void ramfs_init(void) {
    uint8_t sec[512];
    if (ata_read_sector(FS_LBA_START, sec) == 0) {
        uint32_t magic = *(uint32_t*)sec;
        if (magic == FS_MAGIC) {
            uint32_t total_bytes = sizeof(disk_fs_image_t);
            uint32_t total_sectors = (total_bytes + 511) / 512;
            uint8_t* dst = (uint8_t*)&fs_img;
            for (uint32_t s = 0; s < total_sectors; s++) {
                if (ata_read_sector(FS_LBA_START + s, sec) != 0) {
                    ramfs_load_defaults();
                    return;
                }
                uint32_t copy_len = total_bytes - s * 512;
                if (copy_len > 512) copy_len = 512;
                memcpy(dst + s * 512, sec, copy_len);
            }
            persisted_on_disk = true;
            return;
        }
    }
    ramfs_load_defaults();
}

int ramfs_sync(void) {
    fs_img.magic = FS_MAGIC;
    fs_img.version = 1;
    uint32_t total_bytes = sizeof(disk_fs_image_t);
    uint32_t total_sectors = (total_bytes + 511) / 512;
    const uint8_t* src = (const uint8_t*)&fs_img;
    uint8_t sec[512];

    for (uint32_t s = 0; s < total_sectors; s++) {
        memset(sec, 0, 512);
        uint32_t copy_len = total_bytes - s * 512;
        if (copy_len > 512) copy_len = 512;
        memcpy(sec, src + s * 512, copy_len);
        if (ata_write_sector(FS_LBA_START + s, sec) != 0) {
            return -1;
        }
    }
    persisted_on_disk = true;
    return 0;
}

bool ramfs_is_persisted(void) {
    return persisted_on_disk;
}

int ramfs_create(const char* name) {
    if (!name || !name[0]) return -1;
    if (find_file(name) >= 0) return 0;

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!fs_img.files[i].used) {
            fs_img.files[i].used = true;
            strncpy(fs_img.files[i].name, name, RAMFS_MAX_NAME - 1);
            fs_img.files[i].data[0] = '\0';
            fs_img.files[i].size = 0;
            return 0;
        }
    }
    return -1;
}

int ramfs_write(const char* name, const char* content) {
    int idx = find_file(name);
    if (idx < 0) {
        if (ramfs_create(name) < 0) return -1;
        idx = find_file(name);
    }
    size_t len = strlen(content);
    if (len >= RAMFS_MAX_DATA) len = RAMFS_MAX_DATA - 1;
    memcpy(fs_img.files[idx].data, content, len);
    fs_img.files[idx].data[len] = '\0';
    fs_img.files[idx].size = (uint32_t)len;
    return 0;
}

const char* ramfs_read(const char* name) {
    int idx = find_file(name);
    if (idx < 0) return NULL;
    return fs_img.files[idx].data;
}

int ramfs_delete(const char* name) {
    int idx = find_file(name);
    if (idx < 0) return -1;
    fs_img.files[idx].used = false;
    fs_img.files[idx].size = 0;
    return 0;
}

int ramfs_count(void) {
    int c = 0;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (fs_img.files[i].used) c++;
    }
    return c;
}

const ramfs_file_t* ramfs_get(int index) {
    if (index < 0 || index >= RAMFS_MAX_FILES) return NULL;
    if (!fs_img.files[index].used) return NULL;
    return &fs_img.files[index];
}
