# LOOP3: Basic while loops in Drift

This document describes the basic `while` loop implementation in Drift.

A `while` loop repeatedly evaluates a condition and executes its body while that condition is true.

---

## 1. What was added

The `while` loop is connected through the language pipeline:

- the lexer recognizes the `while` keyword
- the AST stores the condition and body
- the statement enum adds `STATEMENT_WHILE`
- the parser reads the condition and statements until `end`
- the interpreter reevaluates the condition before every iteration
- nested loops and conditionals are released through recursive cleanup

The implementation lives in:

- `lexer/core/lexer.c`
- `include/drift/token.h`
- `include/drift/ast.h`
- `include/drift/statement.h`
- `parser/control_flow/while_parser.c`
- `interpreter/loop/while.c`

---

## 2. Syntax

The basic form is:

```drift
var i = 0
while i < 3:
    say i
    i++
end
```

The condition follows `while`, the colon begins the body, and `end` closes the loop.

A `while` loop does not have an initialization clause. Any variable used in its condition or body must therefore be declared before it is used:

```drift
var count = 0
while count < 5:
    say count
    count++
end
```

Using an undeclared variable in the condition produces a runtime error because the expression evaluator cannot find it in the environment.

---

## 3. How the condition works

The condition is stored as expression text by the parser:

```text
while count < 5:
```

The parser does not decide whether `count < 5` is true. That is a runtime decision because values can change during the loop.

The interpreter performs these steps:

1. evaluate the condition
2. convert the result to Drift truthiness
3. stop when it is false
4. execute the body when it is true
5. evaluate the condition again

For the example above, the condition is checked using the current value of `count` on every pass.

---

## 4. Truthiness

The runtime accepts the same basic truthiness rules used by conditional statements:

- `false` is false
- integer `0` is false
- floating-point `0.0` is false
- an empty string is false
- `null` is false
- a nonzero number is true
- a nonempty string is true
- `infinity` is true
- a nonempty array is true

For example:

```drift
var ready = 1
while ready:
    say "running"
    ready = 0
end
```

The body executes once. After the assignment, the next condition check sees integer zero and stops.

---

## 5. Updating the loop

A while loop must change something that eventually makes its condition false. The update is usually placed at the end of the body:

```drift
var i = 0
while i < 4:
    say i
    i++
end
```

The execution order is:

```text
set i to 0
check i < 4
print i
increment i
check i < 4 again
```

If the body never changes the condition toward false, the loop continues indefinitely:

```drift
var i = 0
while i < 1:
    say i
end
```

This is valid syntax, but it is an infinite loop because `i` remains zero.

---

## 6. Runtime behavior

The runtime implementation is in `interpreter/loop/while.c`.

The condition is evaluated through the shared interpreter expression evaluator. This allows conditions to use normal Drift operators, for example:

```drift
var left = 0
var right = 5
while left < right && right != 0:
    say left
    left++
end
```

Each body statement is executed through `interpreter_execute`. Therefore, nested `if`, `repeat`, `for`, and `while` statements keep their normal behavior.

If a body statement returns an error, the while loop stops immediately and returns that error.

---

## 7. AST representation

The parser stores a `WhileStatement` like this:

```c
typedef struct {
    char *condition_text;
    struct Statement *body;
    size_t body_count;
} WhileStatement;
```

`condition_text` is owned by the statement. `body` contains the parsed child statements in source order.

When the statement is destroyed, the condition text and every nested statement are released.

---

## 8. Nested loops

A while loop can contain another loop because its body is parsed as a normal statement list:

```drift
var outer = 0
while outer < 2:
    var inner = 0
    while inner < 2:
        say inner
        inner++
    end
    outer++
end
```

The inner loop consumes its own `end`. The outer loop then continues parsing after the inner block.

---

## 9. Current limits

The basic `while` implementation currently has these limits:

- the condition must be a valid Drift expression
- variables must be declared before the condition reads them
- the block must end with `end`
- `break` and `continue` are not implemented
- the loop does not create a separate lexical scope
- the runtime does not automatically prevent infinite loops

---

## 10. Summary

Drift's basic `while` loop provides:

- condition-based repetition
- runtime reevaluation on every iteration
- normal Drift truthiness
- nested statement support
- separate parser and interpreter modules
- explicit variable declaration requirements

Together with `repeat` and `for`, it gives Drift three different ways to express repeated work.
