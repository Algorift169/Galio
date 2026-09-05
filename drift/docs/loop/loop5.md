# LOOP5: Each loops in Drift

This document describes the `each` loop in Drift. An `each` loop visits every
value in an integer range or one-dimensional array and stores the current
value in its loop variable.

## 1. What was added

The `each` loop is connected through the full language pipeline:

- the lexer recognizes the `each` keyword and the `in` separator
- the AST stores the item name, source expression, and loop body
- the statement enum adds `STATEMENT_EACH`
- the parser reads range and array forms until `end`
- the interpreter evaluates the source and executes the body for every value
- nested `each` statements are released with the other block statements

The implementation lives in:

- `lexer/core/lexer.c`
- `include/drift/token.h`
- `include/drift/ast.h`
- `include/drift/statement.h`
- `parser/control_flow/each_parser.c`
- `interpreter/loop/each.c`

## 2. Range syntax

The range form uses `...` with an exclusive upper endpoint:

```drift
each var i in 0...5:
    say i
end
```

Output:

```text
0
1
2
3
4
```

This is the fixed `each` loop convention: the start is included and the end is
excluded, so `0...5` always means five iterations. An inclusive loop range can
be added later with `..=` without changing the meaning of `...`.

The range endpoints are evaluated at runtime and must produce integers.

## 3. Array syntax

An array expression can be used after `in`:

```drift
var names = ["Alice", "Bob", "Charlie"]

each var name in names:
    say name
end
```

The loop visits the array in index order. Each item is copied into the loop
variable, so the body can use strings, numbers, booleans, or other supported
array element values.

## 4. Runtime behavior

The interpreter evaluates the source once when the loop starts. For a range,
it begins at the start value and increments by one until it reaches the upper
endpoint. For an array, it visits each element from index zero through the
array's current length.

The loop variable is stored in the current environment on every iteration.
The current implementation accepts one-dimensional arrays as `each` sources.
An empty array executes zero body iterations.

`break` exits the `each` loop, while `continue` skips the remaining body and
proceeds to the next range value or array element.

## 5. Summary

Drift's `each` loop provides:

- exclusive-upper integer range iteration
- ordered iteration over one-dimensional arrays
- runtime source evaluation
- normal nested statement behavior
- compatibility with `break` and `continue`