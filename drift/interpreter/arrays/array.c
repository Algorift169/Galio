/* SPDX-License-Identifier: AGPL-3.0-only */
/* Array metadata computes flat offsets from dimensions while keeping bounds checks at the access boundary. */

#include <stdio.h>
#include <stdlib.h>

#include "drift/array.h"
#include "drift/array_value.h"
#include "drift/interpreter.h"
#include "drift/value.h"

static void print_value(const Value *value);

/* Walks nested dimensions recursively while translating each coordinate to a flat offset. 
So the function assumes that the provided indices are valid for the given array dimensions.
In the case of out-of-bounds indices, the function will return 0 to indicate failure, 
and it will not attempt to access the elements array.
Therefore, it is important to ensure that the indices are within the valid range for each dimension
before calling this function. The function uses recursion to handle multi-dimensional arrays,
*/
static void print_array_recursive(const ArrayValue *array, size_t dimension, size_t base_offset)
{
    if (dimension + 1 == array->dimension_count) {
        for (size_t i = 0; i < (size_t)array->dimensions[dimension]; ++i) {
            if (i > 0) {
                printf(" ");
            }
            print_value(&array->elements[base_offset + i]);
        }
        return;
    }

    size_t stride = 1; // Number of flat elements in one child of the current dimension.
    for (size_t i = dimension + 1; i < array->dimension_count; ++i) {
        stride *= (size_t)array->dimensions[i];
    }

    for (size_t i = 0; i < (size_t)array->dimensions[dimension]; ++i) {
        if (i > 0) {
            printf("\n");
            if (dimension == 0 && array->dimension_count > 2) {
                printf("\n");
            }
        }
        print_array_recursive(array, dimension + 1, base_offset + i * stride);
    }
}

/* Prints one-dimensional arrays directly and delegates multidimensional layout to recursion. 
The value is printed using the language's display spelling, which is handled by the print_value 
function. The function checks if the array is dynamic and has a length of 0, in which case it 
returns without printing anything. For one-dimensional arrays, it prints each element separated by commas.
For multidimensional arrays, it calls the print_array_recursive function to handle the nested structure.
param array The ArrayValue to be printed. */
void print_array_value(const ArrayValue *array)
{
    if (array == NULL) {
        return;
    }

    if (array->is_dynamic && array->length == 0) {
        return;
    }

    if (array->dimension_count == 1) {
        for (size_t i = 0; i < array->length; ++i) {
            if (i > 0) {
                printf(",");
            }
            print_value(&array->elements[i]);
        }
        return;
    }

    print_array_recursive(array, 0, 0);
}

/* Validates a coordinate tuple, retrieves its element, and reports access failures.
If we check the indices against the array's dimensions and find that they are out of bounds, 
we set the error flag to 1 and return NULL. If the indices are valid, we calculate the flat index 
using the array_value_get_flat_index function and retrieve the corresponding element from the elements 
array. If the element is NULL, we also set the error flag to 1 and return NULL.
*/
void print_array_element(const ArrayValue *array, const long *indices, size_t index_count)
{
    if (array == NULL) {
        return;
    }

    if (index_count != array->dimension_count) {
        fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", index_count, array->dimension_count);
        return;
    }

    int error = 0;
    const Value *value = array_value_get_element(array, indices, index_count, &error);
    if (value == NULL) {
        if (error) {
            if (index_count == 0) {
                fprintf(stderr, "Runtime Error: Invalid array access.\n");
            } else {
                fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
            }
        }
        return;
    }
    print_value(value);
}

/* Prints each coordinate in a parsed selection while preserving selection order. */
void print_array_selection(const ArrayValue *array, const ArrayAccess *access)
{
    if (array == NULL || access == NULL || !access->is_selection) {
        return;
    }

    if (access->selection_tuple_size != array->dimension_count) {
        fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", access->selection_tuple_size, array->dimension_count);
        return;
    }

    for (size_t i = 0; i < access->selection_count; ++i) {
        const long *indices = access->selection_indices + i * access->selection_tuple_size;
        int error = 0;
        const Value *value = array_value_get_element(array, indices, access->selection_tuple_size, &error);
        if (value == NULL) {
            if (error) {
                fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
            }
            return;
        }

        if (i > 0) {
            printf(" ");
        }
        print_value(value);
    }
}

/* Formats one scalar or nested value using the language's display spelling. */
static void print_value(const Value *value)
{
    if (value == NULL) {
        return;
    }

    switch (value->type) {
        case VALUE_INTEGER:
            printf("%ld", value->integer_value);
            break;
        case VALUE_FLOAT:
            printf("%g", value->float_value);
            break;
        case VALUE_STRING:
            printf("%s", value->string_value ? value->string_value : "");
            break;
        case VALUE_BOOLEAN:
            printf(value->boolean_value ? "true" : "false");
            break;
        case VALUE_NULL:
            printf("null");
            break;
        case VALUE_INFINITY:
            printf("infinity");
            break;
        case VALUE_ARRAY:
            print_array_value(value->array_value);
            break;
        default:
            break;
    }
}
