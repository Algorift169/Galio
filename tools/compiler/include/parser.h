#ifndef GC_PARSER_H
#define GC_PARSER_H

#include "common.h"
#include "lexer.h"
#include "types.h"

typedef enum {
    AST_PROGRAM, AST_FUNC_DECL, AST_VAR_DECL, AST_COMPOUND, AST_EXPR_STMT,
    AST_RETURN, AST_IF, AST_WHILE, AST_FOR, AST_DO_WHILE, AST_BREAK, AST_CONTINUE,
    AST_NUMBER, AST_STRING, AST_CHAR, AST_IDENT, AST_CALL, AST_ASSIGN,
    AST_BINARY, AST_UNARY, AST_INDEX, AST_MEMBER, AST_CAST, AST_SIZEOF,
    AST_TYPEDEF, AST_STRUCT_DECL
} ast_kind_t;
typedef struct symbol symbol_t;
typedef struct ast_node ast_node_t;
typedef struct { ast_node_t **items; u32 count; u32 capacity; } ast_list_t;

struct ast_node {
    ast_kind_t kind;
    gc_type_t *type;
    u32 line;
    union {
        i32 int_value;
        i32 char_value;
        struct { char *name; u32 name_len; symbol_t *sym; } ident;
        struct { char *str; u32 str_len; u32 str_label; } string;
        struct { ast_node_t *base; ast_node_t *index; } index;
        struct { ast_node_t *base; char *name; u32 name_len; u8 arrow; } member;
        struct { char *name; ast_node_t *return_type; ast_list_t params; ast_node_t *body; } func;
        struct { char *name; u32 name_len; ast_node_t *var_type; ast_node_t *init; symbol_t *sym; u8 is_global; } var;
        ast_list_t list;
        struct { ast_node_t *expr; } expr_stmt;
        struct { ast_node_t *expr; } ret;
        struct { ast_node_t *cond; ast_node_t *then_body; ast_node_t *else_body; } branch;
        struct { ast_node_t *init; ast_node_t *cond; ast_node_t *step; ast_node_t *body; } for_loop;
        struct { ast_node_t *body; ast_node_t *cond; } do_loop;
        struct { ast_node_t *callee; ast_list_t args; } call;
        struct { ast_node_t *left; ast_node_t *right; gc_token_kind_t op; } binary;
        struct { ast_node_t *operand; gc_token_kind_t op; u8 is_postfix; } unary;
        struct { char *name; u32 name_len; gc_type_t *type; } typedef_decl;
    } as;
};

typedef enum { SYM_VAR, SYM_FUNC, SYM_TYPE } symbol_kind_t;
struct symbol {
    char *name; u32 name_len; symbol_kind_t kind; gc_type_t *type;
    u32 scope_level; i32 stack_offset; u8 is_global; u8 is_defined;
    struct symbol *next;
};
#define GC_SCOPE_BUCKETS 64
#define MAX_SCOPE_DEPTH 64
typedef struct { symbol_t *buckets[GC_SCOPE_BUCKETS]; u32 scope_level; } scope_t;
typedef struct { scope_t scopes[MAX_SCOPE_DEPTH]; u32 scope_count; } symtab_t;
typedef struct { gc_lexer_t *lexer; gc_token_t current; gc_token_t peek; u32 error_count; char error_msg[256]; symtab_t *symtab; } gc_parser_t;

ast_node_t *ast_new(ast_kind_t kind);
ast_node_t *ast_number(i32 value);
ast_node_t *ast_ident(const char *name, u32 len);
ast_node_t *ast_binary(ast_kind_t kind, gc_token_kind_t op, ast_node_t *left, ast_node_t *right);
ast_node_t *ast_unary(ast_kind_t kind, gc_token_kind_t op, ast_node_t *operand);
void ast_free(ast_node_t *node);
void symtab_init(symtab_t *symtab);
void symtab_push_scope(symtab_t *symtab);
void symtab_pop_scope(symtab_t *symtab);
symbol_t *symtab_lookup(symtab_t *symtab, const char *name, u32 len);
symbol_t *symtab_lookup_current(symtab_t *symtab, const char *name, u32 len);
symbol_t *symtab_insert(symtab_t *symtab, const char *name, u32 len, symbol_kind_t kind);
int gc_parser_init(gc_parser_t *parser, gc_lexer_t *lexer, symtab_t *symtab);
ast_node_t *gc_parse_program(gc_parser_t *parser);
int gc_typecheck(ast_node_t *node, symtab_t *symtab);

#endif
