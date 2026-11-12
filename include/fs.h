#ifndef FS_H
#define FS_H

#include "types.h"

#define RAMFS_MAX_FILES 16
#define RAMFS_MAX_NAME  32
#define RAMFS_MAX_DATA  512

typedef struct {
    char name[RAMFS_MAX_NAME];
    char data[RAMFS_MAX_DATA];
    uint32_t size;
    bool used;
} __attribute__((packed)) ramfs_file_t;

void ramfs_init(void);
int  ramfs_create(const char* name);
int  ramfs_write(const char* name, const char* content);
const char* ramfs_read(const char* name);
int  ramfs_delete(const char* name);
int  ramfs_count(void);
const ramfs_file_t* ramfs_get(int index);
int  ramfs_sync(void);
bool ramfs_is_persisted(void);

#endif
