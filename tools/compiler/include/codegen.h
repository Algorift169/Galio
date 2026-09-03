#ifndef GC_CODEGEN_H
#define GC_CODEGEN_H

#include "common.h"
int gc_codegen_source(const char *source, u8 **code, u32 *code_size, u8 **data, u32 *data_size, u32 *entry_offset);
#endif
