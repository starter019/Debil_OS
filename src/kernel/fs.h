/* DEBIL OS - tiny in-memory filesystem */
#ifndef DEBIL_FS_H
#define DEBIL_FS_H

#include "types.h"

#define FS_MAX_FILES 24
#define FS_MAX_NAME  32
#define FS_MAX_DATA  4096

typedef struct {
    uint8_t  used;
    char     name[FS_MAX_NAME];
    uint32_t size;
    uint8_t  data[FS_MAX_DATA];
} fs_file_t;

void fs_init(void);
uint32_t fs_count(void);
fs_file_t *fs_get(uint32_t index);
fs_file_t *fs_find(const char *name);
fs_file_t *fs_create(const char *name);
int  fs_delete(const char *name);
int  fs_write(const char *name, const void *data, uint32_t len);
int  fs_append(const char *name, const void *data, uint32_t len);
int  fs_rename(const char *old_name, const char *new_name);
uint32_t fs_used_bytes(void);
uint32_t fs_capacity_bytes(void);
int  fs_valid_name(const char *name);

#endif
