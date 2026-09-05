# Arithmetic Operators

Drift supports the core arithmetic operators:

- `+` addition
- `-` subtraction
- `*` multiplication
- `/` division
- `%` modulo

## Syntax

```drift
var a = 5 + 3
var b = 10 - 4
var c = 6 * 7
var d = 20 / 4
var e = 17 % 5
```

## Behavior

These operators require numeric operands. Integer and float values are accepted. Boolean values are treated as numeric-like at runtime when appropriate.

Examples:

```drift
var x = 12.5 + 2.5
say x

var y = 9 / 3
say y

var z = 13 % 4
say z
```

## Output

```text
15.0
3.0
1.0
```

## Implementation notes

The arithmetic evaluator dispatches based on the operator token and performs numeric validation before applying the operation. Division by zero is rejected with a runtime error.
