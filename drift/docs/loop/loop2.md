# LOOP2: Basic for loops in Drift

This document describes the basic `for` loop implementation in Drift.

The loop follows the familiar three-part structure:

1. initialization
2. condition
3. increment or decrement

The implementation stores these clauses in the syntax tree and evaluates them during execution.

---

## 1. What was added

The `for` loop is connected through the full Drift pipeline:

- the lexer recognizes the `for` keyword
- the AST stores initialization, condition, increment, and body data
- the statement enum adds `STATEMENT_FOR`
- the parser reads the three loop clauses and the block body
- the interpreter executes the clauses in the correct order
- nested `for` loops can be cleaned up with other statement types

The implementation lives in:

- `lexer/core/lexer.c`
- `include/drift/token.h`
- `include/drift/ast.h`
- `include/drift/statement.h`
- `parser/control_flow/for_parser.c`
- `interpreter/loop/for.c`

---

## 2. Required syntax

The basic form is:

```drift
for var i = 0, i < 3, i++:
    say i
end
```

The three parts are separated by commas:

```text
for initialization, condition, increment_or_decrement:
```

The body begins after `:` and ends at `end`.

The `var` keyword is important. It explicitly declares the loop counter:

```drift
for var i = 0, i < 5, i++:
    say i
end
```

A loop may also use a counter that was declared before the loop:

```drift
var i = 0
for i = 0, i < 5, i++:
    say i
end
```

A counter cannot be used without either `var` or a previous declaration:

```drift
for i = 0, i < 5, i++:
    say i
end
```

This produces a runtime error because `i` does not exist before the loop starts.

The parser also accepts an optional parenthesized form:

```drift
for (var i = 0, i < 5, i++):
    say i
end
```

The unparenthesized form is the normal documented syntax.

---

## 3. Meaning of each clause

### Initialization

Initialization runs once, before the first condition check:

```drift
for var i = 0, i < 3, i++:
    say i
end
```

Here, `i = 0` creates or sets the counter before the body can run.

The initialization is evaluated at runtime, so its right side can use values already present in the environment:

```drift
var start = 2
for var i = start, i < 5, i++:
    say i
end
```

### Condition

The condition is evaluated before every body execution:

```drift
for var i = 0, i < 3, i++:
    say i
end
```

The values of `i` are checked as:

```text
i = 0: true
 i = 1: true
 i = 2: true
 i = 3: false
```

When the condition becomes false, the body is skipped and the loop ends.

### Increment or decrement

The increment runs after the complete body finishes:

```drift
for var i = 0, i < 3, i++:
    say i
end
```

The order is:

```text
initialize i
check condition
run body
increment i
check condition again
```

The implementation supports the basic forms:

```drift
for var i = 0, i < 5, i++:
    say i
end
```

```drift
for var i = 5, i > 0, i--:
    say i
end
```

It also supports compound arithmetic updates:

```drift
for var i = 0, i < 10, i += 2:
    say i
end
```

and:

```drift
for var i = 10, i > 0, i -= 2:
    say i
end
```

---

## 4. Runtime execution order

The runtime implementation in `interpreter/loop/for.c` follows this sequence:

1. validate the `ForStatement`
2. execute the initialization once
3. evaluate the condition
4. stop if the condition is false
5. execute each statement in the body
6. execute the increment or decrement
7. return to step 3

The body is executed through the main interpreter entry point. This means that declarations, printing, conditionals, repeat loops, nested `for` loops, and nested `while` loops use their normal behavior inside a `for` loop.

If a body statement fails, the `for` loop stops immediately and returns that error.

---

## 5. AST representation

The parser stores a `ForStatement` like this:

```c
typedef struct {
    char *init_text;
    int declares_counter;
    char *condition_text;
    char *increment_text;
    struct Statement *body;
    size_t body_count;
} ForStatement;
```

The expressions remain as text instead of being evaluated during parsing. This is necessary because the condition must be evaluated repeatedly and the runtime environment may change between iterations.

`declares_counter` records whether the header used `var`.

---

## 6. Examples

### Counting upward

```drift
for var i = 0, i < 4, i++:
    say i
end
```

Output:

```text
0
1
2
3
```

### Counting downward

```drift
for var i = 3, i > 0, i--:
    say i
end
```

Output:

```text
3
2
1
```

### Using a previously declared counter

```drift
var i = 0
for i = 0, i < 3, i++:
    say i
end
```

### Nested loop

```drift
for var outer = 0, outer < 2, outer++:
    for var inner = 0, inner < 2, inner++:
        say inner
    end
end
```

Each loop owns its parsed body and its own counter update sequence.

---

## 7. Current limits

The basic `for` implementation currently has these limits:

- the initialization clause must be an assignment such as `i = 0`
- the condition must be a valid Drift expression
- the increment must update the counter with `++`, `--`, `+=`, or `-=`
- the counter must be declared with `var` or exist before the loop
- the block must end with `end`
- `break` and `continue` are not implemented
- the implementation does not create a separate lexical scope for the counter

---

## 8. Summary

Drift's basic `for` loop provides:

- explicit initialization
- a condition checked before each iteration
- increment and decrement support
- previously declared or explicitly declared counters
- nested statement execution
- separate parser and interpreter modules

This creates a simple, predictable loop structure while preserving Drift's existing parser and environment model.
