#include "types.h"
#include "heap.h"
#include "string.h"

static gc_type_t builtin_void = { .kind = TYPE_VOID, .size = 0, .align = 1 };
static gc_type_t builtin_int = { .kind = TYPE_INT, .size = 4, .align = 4 };
static gc_type_t builtin_char = { .kind = TYPE_CHAR, .size = 1, .align = 1 };
gc_type_t *type_void = &builtin_void;
gc_type_t *type_int = &builtin_int;
gc_type_t *type_char = &builtin_char;

gc_type_t *type_new(type_kind_t kind) {
    gc_type_t *type = (gc_type_t *)kmalloc(sizeof(gc_type_t));
    if (!type) return 0;
    memset(type, 0, sizeof(*type)); type->kind = kind;
    type->align = kind == TYPE_CHAR ? 1 : 4;
    type->size = kind == TYPE_VOID ? 0 : (kind == TYPE_CHAR ? 1 : 4);
    return type;
}
gc_type_t *type_pointer(gc_type_t *base) { gc_type_t *type = type_new(TYPE_POINTER); if (type) { type->size = 4; type->as.ptr.base = base; } return type; }
gc_type_t *type_array(gc_type_t *base, u32 count) { gc_type_t *type = type_new(TYPE_ARRAY); if (type) { type->size = base->size * count; type->as.ptr.base = base; type->as.ptr.array_size = count; } return type; }
gc_type_t *type_function(gc_type_t *ret, gc_type_t **params, u32 count) { gc_type_t *type = type_new(TYPE_FUNCTION); if (type) { type->size = 4; type->as.func.ret = ret; type->as.func.params = params; type->as.func.param_count = count; } return type; }
gc_type_t *type_struct(const char *name, u8 is_union) { gc_type_t *type = type_new(is_union ? TYPE_UNION : TYPE_STRUCT); u32 n = 0; if (!type) return 0; while (name && name[n]) n++; type->as.struc.name = (char *)kmalloc(n + 1); if (type->as.struc.name) { memcpy(type->as.struc.name, name, n); type->as.struc.name[n] = 0; } type->size = 0; return type; }
int type_add_member(gc_type_t *type, const char *name, gc_type_t *member_type) { u32 i, n = 0; if (!type || (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION)) return -1; while (name[n]) n++; for (i = 0; i < type->as.struc.member_count; i++) if (!strcmp(type->as.struc.members[i].name, name)) return -1; type->as.struc.members = (typeof(type->as.struc.members))krealloc(type->as.struc.members, (type->as.struc.member_count + 1) * sizeof(*type->as.struc.members)); if (!type->as.struc.members) return -1; type->as.struc.members[type->as.struc.member_count].name = (char *)kmalloc(n + 1); if (!type->as.struc.members[type->as.struc.member_count].name) return -1; memcpy(type->as.struc.members[type->as.struc.member_count].name, name, n + 1); type->as.struc.members[type->as.struc.member_count].type = member_type; type->as.struc.members[type->as.struc.member_count].offset = type->kind == TYPE_UNION ? 0 : type->size; if (type->kind == TYPE_UNION) { if (member_type->size > type->size) type->size = member_type->size; } else type->size += member_type->size; type->as.struc.member_count++; return 0; }
gc_type_t *type_member(gc_type_t *type, const char *name) { u32 i; if (!type) return 0; for (i = 0; i < type->as.struc.member_count; i++) if (!strcmp(type->as.struc.members[i].name, name)) return type->as.struc.members[i].type; return 0; }
u32 type_size(gc_type_t *type) { return type ? type->size : 0; }
u8 type_equal(gc_type_t *a, gc_type_t *b) { u32 i; if (a == b) return 1; if (!a || !b || a->kind != b->kind) return 0; if (a->kind == TYPE_POINTER || a->kind == TYPE_ARRAY) return type_equal(a->as.ptr.base, b->as.ptr.base) && (a->kind != TYPE_ARRAY || a->as.ptr.array_size == b->as.ptr.array_size); if (a->kind == TYPE_FUNCTION) { if (!type_equal(a->as.func.ret, b->as.func.ret) || a->as.func.param_count != b->as.func.param_count) return 0; for (i = 0; i < a->as.func.param_count; i++) if (!type_equal(a->as.func.params[i], b->as.func.params[i])) return 0; return 1; } return 1; }
u8 type_is_integer(gc_type_t *type) { return type && (type->kind == TYPE_INT || type->kind == TYPE_CHAR); }
u8 type_is_arithmetic(gc_type_t *type) { return type_is_integer(type); }
