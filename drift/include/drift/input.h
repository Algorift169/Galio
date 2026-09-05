/* SPDX-License-Identifier: AGPL-3.0-only */
/* Input APIs collect user text and convert it into values without exposing buffer ownership. */

#ifndef DRIFT_INPUT_H
#define DRIFT_INPUT_H

#include <stddef.h>

#include "drift/value.h"

char *drift_read_line_from_stdin(const char *prompt);
/* Converts textual input into the first matching language value type. */
int drift_parse_input_value(const char *text, Value *out_value);
/* Reads, parses, and returns the value associated with an input target. */
int drift_prompt_and_store(const char *prompt, const char *target_name, Value *out_value);

#endif
