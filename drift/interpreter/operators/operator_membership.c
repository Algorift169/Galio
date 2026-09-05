/* SPDX-License-Identifier: AGPL-3.0-only */
/* Membership scans arrays or strings and compares compatible element types without stealing operands. */

#include <stdio.h>
#include <string.h>

#include "drift/operator.h"
#include "drift/array_value.h"

/* Tests whether a value occurs in the supported container types. */
Value operator_apply(OperatorType op, const Value *container, const Value *member)
{
    /* Arrays use element-by-element compatible-type comparisons, while strings
       use substring search. Neither operand is modified or transferred. */
    Value result = value_create_boolean(0);

    if (op == OPERATOR_IN) {
        if (container == NULL) {
            fprintf(stderr, "Runtime Error: Membership operator requires a container.\n");
            return result;
        }

        if (container->type == VALUE_ARRAY && member != NULL) {
            ArrayValue *arr = container->array_value;
            if (arr != NULL) {
                // Stop immediately when a compatible matching element is found.
                for (size_t i = 0; i < arr->length; i++) {
                    Value *elem = &arr->elements[i];

                    if (member->type == VALUE_INTEGER && elem->type == VALUE_INTEGER) {
                        if (member->integer_value == elem->integer_value) {
                            return value_create_boolean(1);
                        }
                    } else if (member->type == VALUE_STRING && elem->type == VALUE_STRING) {
                        const char *member_str = member->string_value ? member->string_value : "";
                        const char *elem_str = elem->string_value ? elem->string_value : "";
                        if (strcmp(member_str, elem_str) == 0) {
                            return value_create_boolean(1);
                        }
                    } else if (member->type == VALUE_FLOAT && elem->type == VALUE_FLOAT) {
                        if (member->float_value == elem->float_value) {
                            return value_create_boolean(1);
                        }
                    }
                }
            }
        } else if (container->type == VALUE_STRING && member != NULL) {
            // String membership means that the member text occurs as a substring.
            if (member->type == VALUE_STRING) {
                const char *container_str = container->string_value ? container->string_value : "";
                const char *member_str = member->string_value ? member->string_value : "";
                if (strstr(container_str, member_str) != NULL) {
                    return value_create_boolean(1);
                }
            }
        }
    }

    return result;
}
