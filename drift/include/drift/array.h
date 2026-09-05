/* SPDX-License-Identifier: AGPL-3.0-only */
/* Public array metadata and traversal helpers used by parsing and execution. */

#ifndef DRIFT_ARRAY_H
#define DRIFT_ARRAY_H

#include <stddef.h>

#include "drift/value.h"

struct Parser;

typedef struct {
    char *name;
    int is_whole_array;
    size_t whole_array_dimension_count;
    int is_selection;
    size_t index_count;
    long *indices;
    char **index_names;
    size_t selection_count;
    size_t selection_tuple_size;
    long *selection_indices;
    int *selection_breaks;
} ArrayAccess;

void array_access_init(ArrayAccess *access);
void array_access_free(ArrayAccess *access);
int parse_array_access(struct Parser *parser, ArrayAccess *access);
int parse_select_access(struct Parser *parser, ArrayAccess *access);
Value parse_array_declaration(struct Parser *parser, int *error);
void print_array_value(const ArrayValue *array);
void print_array_element(const ArrayValue *array, const long *indices, size_t index_count);
void print_array_selection(const ArrayValue *array, const ArrayAccess *access);

#endif
