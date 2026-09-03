#ifndef GC_CODEGEN_H
#define GC_CODEGEN_H
#include "common.h"
#include "parser.h"
typedef struct { char *buf; u32 size; u32 capacity; u32 labels; u32 strings; u32 current_end; } codegen_t;
void codegen_init(codegen_t *cg);
void codegen_emit(codegen_t *cg, const char *format, ...);
int gc_codegen_ast(ast_node_t *ast, codegen_t *cg);
int gc_codegen_source(const char *source, u8 **code, u32 *code_size, u8 **data, u32 *data_size, u32 *entry_offset);
#endif
