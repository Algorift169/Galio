#ifndef FS_BROWSER_H
#define FS_BROWSER_H

#include "common.h"

/* Filesystem browser for left sidebar display */

typedef struct {
    char path[256];
    u8 is_dir;
} fs_entry_t;

typedef struct {
    fs_entry_t *entries;
    u32 count;
    u32 scroll_offset;
    u32 max_entries;
} fs_browser_t;

/* Initialize filesystem browser */
void fs_browser_init(void);

/* Load directory listing from given path */
u32 fs_browser_load_dir(const char *path);

/* Get filesystem browser state */
fs_browser_t *fs_browser_get(void);

/* Scroll up in the filesystem list */
void fs_browser_scroll_up(void);

/* Scroll down in the filesystem list */
void fs_browser_scroll_down(void);

/* Draw filesystem browser at specified position */
void fs_browser_draw(int x, int y, int width, int height);

/* Get number of entries in current directory */
u32 fs_browser_get_count(void);

/* Get scroll offset */
u32 fs_browser_get_scroll_offset(void);

#endif /* FS_BROWSER_H */
