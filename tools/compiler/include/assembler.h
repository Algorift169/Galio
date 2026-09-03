#ifndef GC_ASSEMBLER_H
#define GC_ASSEMBLER_H

#include "common.h"

typedef struct { char *name; u32 address; } label_entry_t;
typedef struct { label_entry_t *entries; u32 count; u32 capacity; } label_table_t;
void labels_init(label_table_t *labels);
void labels_add(label_table_t *labels, const char *name, u32 address);
u32 labels_find(label_table_t *labels, const char *name);

int gc_assemble(const char *source, u8 **code, u32 *code_size, u8 **data, u32 *data_size, u32 *entry_offset);

#endif
