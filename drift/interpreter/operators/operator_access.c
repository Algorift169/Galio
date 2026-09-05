/* SPDX-License-Identifier: AGPL-3.0-only */
/* Access evaluation converts multidimensional indices to array offsets and returns copied values. */

#include <stdio.h>
#include <stdlib.h>

#include "drift/operator.h"
#include "drift/array_value.h"

/*
Creates an inclusive integer range as a dynamic array. The temporary element
buffer is released after the array constructor copies or adopts its values.
*/
Value operator_apply(OperatorType op, const Value *container, const Value *index)
{
    Value result = value_create_null(); // Initialize result to null

    if (op == OPERATOR_RANGE) {
        if (container == NULL || index == NULL) { // Check for null operands
            fprintf(stderr, "Runtime Error: Range operator requires start and end values.\n");
            return result; 
        }

            // Check if both operands are integers before calculating the length.
        if (container->type == VALUE_INTEGER && index->type == VALUE_INTEGER) {
            long start = container->integer_value;
            long end = index->integer_value;
            size_t count = 0;
            Value *elements = NULL;

            // Create an ascending or descending array of integers from the endpoints.
            if (start <= end) {
                count = (size_t)(end - start + 1);
                elements = (Value *)malloc(count * sizeof(Value));
                if (elements != NULL) {
                    for (size_t i = 0; i < count; i++) {
                        elements[i] = value_create_integer(start + (long)i);
                    }
                }
            } else {
                count = (size_t)(start - end + 1); // Create a reverse range if start > end
                elements = (Value *)malloc(count * sizeof(Value)); 
                if (elements != NULL) {
                    for (size_t i = 0; i < count; i++) {
                        elements[i] = value_create_integer(start - (long)i); // Create a reverse range if start > end
                    }
                }
            }

            // Release the temporary values after constructing the dynamic array.
            if (elements != NULL) {
                ArrayValue *arr = array_value_create_dynamic_from_values(VALUE_INTEGER, elements, count); // Create a dynamic array from the generated elements
                if (arr != NULL) {
                    result = value_create_array(arr);
                }
                for (size_t i = 0; i < count; i++) {
                    value_free(&elements[i]);
                }
                free(elements); // Free the temporary elements array
            } else {
                fprintf(stderr, "Runtime Error: Out of memory creating range.\n");
            }
        } else {
            fprintf(stderr, "Runtime Error: Range operator requires integer operands.\n");
        }
    }

    // result = the Value object representing the result of the range 
    // operation. It will be an array of integers if the operation was 
    // successful, or a null value if there was an error. 
    // The caller is responsible for checking the type of the result and
    // handling it appropriately, including freeing any allocated memory
    // when the result is no longer needed.
    return result;
}