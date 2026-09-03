#ifndef GC_H
#define GC_H

#include "common.h"

#define GC_OK 0
#define GC_ERR_FILE_OPEN -1
#define GC_ERR_FILE_READ -2
#define GC_ERR_FILE_WRITE -3
#define GC_ERR_PARSE -4
#define GC_ERR_SYNTAX -5
#define GC_ERR_TYPE -6
#define GC_ERR_UNDEFINED -7
#define GC_ERR_REDEFINED -8
#define GC_ERR_MEMORY -9
#define GC_ERR_ELF -10

int gc_compile(const char *source_path, const char *output_path);

#endif
