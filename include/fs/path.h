#ifndef PATH_H
#define PATH_H

#include "common.h"

u8 path_is_absolute(const char *path);
char *path_normalize(const char *path, char *out, u32 out_size);
char *path_resolve(const char *cwd, const char *path, char *out, u32 out_size);
char *path_join(const char *base, const char *relative, char *out, u32 out_size);
char *path_parent(const char *path, char *out, u32 out_size);
char *path_basename(const char *path, char *out, u32 out_size);

#endif /* PATH_H */
