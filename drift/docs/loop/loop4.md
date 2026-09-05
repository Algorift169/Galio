# LOOP4: Break and continue statements in Drift

This document describes the `break` and `continue` statement implementation in Drift.

The `break` statement exits the innermost enclosing loop immediately, while `continue` skips the remainder of the current iteration body and proceeds directly to the next iteration or condition evaluation.

---

## 1. What was added

The `break` and `continue` loop control features are connected through the full compiler and interpreter pipeline:

- the lexer recognizes `break` (`TOKEN_BREAK`) and `continue` (`TOKEN_CONTINUE`) keywords
- the statement enum adds `STATEMENT_BREAK` and `STATEMENT_CONTINUE`
- control flow status constants (`DRIFT_EXECUTION_BREAK = 2`, `DRIFT_EXECUTION_CONTINUE = 3`) were introduced for control propagation
- the parser recognizes `break` and `continue` statements inside block bodies
- all loop runners (`while`, `for`, `repeat`) handle status codes and correctly terminate or skip iterations
- top-level execution catches orphaned `break` or `continue` statements outside loops and emits a runtime error

The core implementation lives in:

- `lexer/core/lexer.c`
- `include/drift/token.h`
- `include/drift/statement.h`
- `include/drift/control_flow.h`
- `parser/core/parser.c`
- `parser/control_flow/while_parser.c`
- `parser/control_flow/for_parser.c`
- `parser/control_flow/repeat_parser.c`
- `interpreter/loop/break.c`
- `interpreter/loop/continue.c`
- `interpreter/loop/while.c`
- `interpreter/loop/for.c`
- `interpreter/loop/repeat.c`
- `main.c`

---

## 2. Syntax and usage

Both statements are standalone control primitives requiring no parameters or operands:

```drift
break
```

```drift
continue
```

### Using `break` in loops

`break` terminates the enclosing loop immediately:

```drift
var while_index = 0
while while_index < 5:
    while_index++
    if while_index == 3:
        break
    say while_index
end
```

Output:
```text
1
2
```

### Using `continue` in loops

`continue` skips the rest of the loop body for the current iteration:

```drift
for var for_index = 0, for_index < 4, for_index++:
    if for_index == 2:
        continue
    say for_index
end
```

Output:
```text
0
1
3
```

---

## 3. Propagation and Control Flow Status Codes

Execution dispatches statements through `interpreter_execute`. Standard statements return `DRIFT_EXECUTION_OK` (`0`), while runtime errors return `1` (`DRIFT_EXECUTION_ERROR`).

To propagate control flow signals across nested statements (such as `if` blocks), status codes were defined in `include/drift/control_flow.h`:

- `DRIFT_EXECUTION_BREAK` (`2`)
- `DRIFT_EXECUTION_CONTINUE` (`3`)

When `break` or `continue` is executed:
1. `interpreter_execute_break` or `interpreter_execute_continue` returns `DRIFT_EXECUTION_BREAK` or `DRIFT_EXECUTION_CONTINUE`.
2. Parent statement structures (such as `if` branches) propagate non-zero execution statuses upward without modification.
3. The innermost enclosing loop runner intercepts the status code:
   - On `DRIFT_EXECUTION_BREAK`: the loop sets a break flag and exits its host iteration loop.
   - On `DRIFT_EXECUTION_CONTINUE`: the loop runner breaks out of body execution and advances to the loop increment (`for`) or condition re-evaluation (`while`, `repeat`).

---

## 4. Behavior across Loop Types

### While Loops (`interpreter/loop/while.c`)
- **`break`**: Exits the `while (1)` loop immediately.
- **`continue`**: Skips remaining statements in the body and jumps back to reevaluate the `while` condition expression.

### For Loops (`interpreter/loop/for.c`)
- **`break`**: Exits the outer loop immediately without executing the increment clause.
- **`continue`**: Skips remaining statements in the body, executes the increment expression (`execute_for_increment`), and reevaluates the loop condition.

### Repeat Loops (`interpreter/loop/repeat.c`)
- **`break`**: Exits range iteration (`start` to `end` / infinite) immediately.
- **`continue`**: Skips remaining body statements and proceeds to the next index update in the range.

---

## 5. Parser Integration Details

During parsing of loop block bodies (`while_parser.c`, `for_parser.c`), statement terminators (`TOKEN_NEWLINE` / `TOKEN_SEMICOLON`) are processed before checking block indentation boundaries. This ensures statements on newlines within indented blocks are appended into the loop statement list correctly (`body_count > 0`), enabling proper loop control execution.

---

## 6. Orphaned Control Statements

If a `break` or `continue` statement is executed outside of any enclosing loop context (e.g. at file scope), the status signal propagates up to `main.c` / `execute_source`.

`main.c` intercepts unhandled `DRIFT_EXECUTION_BREAK` and `DRIFT_EXECUTION_CONTINUE` signals at top-level and emits a runtime error:

```text
Runtime Error: Loop control statement used outside a loop.
```

---

## 7. Summary

Drift's `break` and `continue` statements provide:

- Immediate loop termination (`break`)
- Iteration skipping with automatic step/increment handling (`continue`)
- Transparent status code propagation (`DRIFT_EXECUTION_BREAK`, `DRIFT_EXECUTION_CONTINUE`) across nested blocks
- Comprehensive support in `while`, `for`, and `repeat` loops
- Top-level runtime error checking for misuse outside loops
