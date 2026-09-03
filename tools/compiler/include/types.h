#ifndef GC_TYPES_H
#define GC_TYPES_H

#include "common.h"

typedef enum { TYPE_VOID, TYPE_INT, TYPE_CHAR, TYPE_POINTER, TYPE_ARRAY, TYPE_FUNCTION, TYPE_STRUCT, TYPE_UNION } type_kind_t;
typedef struct gc_type gc_type_t;

struct gc_type {
    type_kind_t kind;
    u32 size;
    u32 align;
    union {
        struct { gc_type_t *base; u32 array_size; } ptr;
        struct { gc_type_t *ret; gc_type_t **params; u32 param_count; } func;
        struct { char *name; struct { char *name; gc_type_t *type; u32 offset; } *members; u32 member_count; } struc;
    } as;
};

extern gc_type_t *type_void;
extern gc_type_t *type_int;
extern gc_type_t *type_char;
gc_type_t *type_new(type_kind_t kind);
gc_type_t *type_pointer(gc_type_t *base);
gc_type_t *type_array(gc_type_t *base, u32 count);
gc_type_t *type_function(gc_type_t *ret, gc_type_t **params, u32 count);
gc_type_t *type_struct(const char *name, u8 is_union);
int type_add_member(gc_type_t *type, const char *name, gc_type_t *member_type);
gc_type_t *type_member(gc_type_t *type, const char *name);
u32 type_size(gc_type_t *type);
u8 type_equal(gc_type_t *a, gc_type_t *b);
u8 type_is_integer(gc_type_t *type);
u8 type_is_arithmetic(gc_type_t *type);

#endif