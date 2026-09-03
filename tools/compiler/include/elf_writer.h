#ifndef GC_ELF_WRITER_H
#define GC_ELF_WRITER_H

#include "common.h"

int elf_write(const char *path, const u8 *code, u32 code_size, const u8 *data, u32 data_size, u32 entry_point);

#endif
