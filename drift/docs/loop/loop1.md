# LOOP1: Repeat loops in Drift

This document describes the loop implementation added in commit `71fb478c`:

> Add repeat loop support with range/step semantics and parser integration

The change introduces a first-class `repeat` statement to Drift, covering tokenization, AST representation, parsing, and runtime execution.

---

## 1. What was added

The implementation adds a new loop construct to the language and wires it through the full compiler pipeline:

- the lexer recognizes the `repeat` keyword
- the AST adds a `RepeatStatement` structure
- the statement enum adds `STATEMENT_REPEAT`
- the parser builds a repeat block from header + body + `end`
- the interpreter executes the loop body in order

The core implementation lives in:

- `lexer/lexer.c`
- `include/drift/ast.h`
- `include/drift/statement.h`
- `parser/repeat_parser.c`
- `interpreter/intptr.c`

---

## 2. Supported syntax

The loop grammar is centered around a counter name and a range expression:

```drift
repeat var i (0...10):
    say i
end
```

The `var` keyword is optional when using an existing variable:

```drift
repeat i (0..<10, i+2):
    say i
end
```

The supported range forms include:

### Inclusive range

```drift
repeat var i (0...6):
    say i
end
```

This runs for `0, 1, 2, 3, 4, 5, 6`.

### Exclusive upper bound

```drift
repeat i (0..<10):
    say i
end
```

This runs for `0` through `9`.

### Exclusive lower bound

```drift
repeat i (10..>0):
    say i
end
```

This runs for `10` down to `1`.

### Step control

```drift
repeat var i (0...6, i+2):
    say i
end
```

The optional step expression is separated by a comma. The implementation also accepts counter-based step patterns such as:

```drift
repeat i (0..<10, i++):
    say i
end
```

and:

```drift
repeat i (10...0, i--):
    say i
end
```

### Infinite repeat

The parser also accepts a loop without a concrete end bound:

```drift
repeat var i(...):
    say i
end
```

This starts at `0` and keeps incrementing based on the step direction. If no step is provided, the runtime defaults to `+1`.

---

## 3. Parser model

The new parser logic is implemented in `parser/repeat_parser.c`.

The flow is:

1. consume `repeat`
2. optionally consume `var`
3. read the loop counter name
4. parse either:
   - a parenthesized range expression, or
   - an infinite loop header `(...)`
5. expect a `:` after the header
6. parse the body until `end`

The parser stores the loop header in a `RepeatStatement` object:

```c
typedef struct {
    char *counter_name;
    int has_range;
    int has_step;
    int is_infinite;
    int is_exclusive_upper;
    int is_exclusive_lower;
    char *start_text;
    char *end_text;
    char *step_text;
    struct Statement *body;
    size_t body_count;
} RepeatStatement;
```

This structure captures the exact range metadata needed at runtime.

---

## 4. Runtime behavior

The execution logic is in `interpreter/intptr.c`.

### Counter variable

Each iteration assigns the current integer counter value to the loop variable:

```c
Value counter_value = value_create_integer(i);
environment_set(environment, repeat_statement->counter_name, &counter_value);
```

This makes the counter available inside the loop body.

### Integer evaluation

The runtime evaluates the loop bounds and step value as integer expressions:

- `start_text` is evaluated to an integer
- `end_text` is evaluated to an integer
- `step_text` is evaluated to an integer, if present

If a step resolves to zero, execution stops with a runtime error.

### Direction and bounds

The loop direction is inferred when a step is not explicitly supplied:

- if `start <= end`, the default step is `+1`
- if `start > end`, the default step is `-1`

For exclusive bounds:

- `..<` uses upper-bound exclusivity
- `..>` uses lower-bound exclusivity

The loop condition is then applied as:

- `i < end` for exclusive upper bounds
- `i > end` for exclusive lower bounds
- or the standard inclusive comparison otherwise

---

## 5. Examples from the implementation

The test suite in `tests/` demonstrates the actual runtime contract:

### Basic inclusive range

```drift
var n = 3
repeat var i (0...n):
    say i
end
```

### Step with start/end expressions

```drift
repeat var i (0...6, i+2):
    say i
end
```

### Descending range

```drift
repeat i (n...0, i--):
    say i
end
```

### Exclusive upper bound

```drift
var i
repeat i (0..<n, i++):
    say n + i
end
```

### Infinite stream

```drift
var n = 5
repeat var i(...):
    say n + i
end
```

---

## 6. Edge cases and current limits

The implementation supports the core loop feature set, but it is intentionally narrow:

- `repeat` without a range is currently rejected
- a step of `0` is invalid
- the language does not yet add `break` or `continue`
- infinite loops are supported only in the current `repeat (...)` shape
- the runtime expects integer-valued bounds and steps

These limitations are visible in the interpreter checks and in the runtime error messages added during this work.

---

## 7. Summary

Commit `71fb478c` adds a working `repeat` loop system to Drift with:

- range-based iteration
- optional step expressions
- inclusive and exclusive bound handling
- infinite loop support
- parser and interpreter integration
This makes `repeat` the first full looping primitive in Drift and lays the foundation for more advanced control flow in later commits.
