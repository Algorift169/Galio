#ifndef GUI_PNG_H
#define GUI_PNG_H

#include "../../include/common.h"

typedef struct {
    u32 width;
    u32 height;
    u8 *pixels;
} png_image_t;

u8 png_load_from_vfs(const char *path, png_image_t *image);
void png_free(png_image_t *image);

#endif