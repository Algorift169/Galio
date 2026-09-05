# Logical Operators

Drift supports the following logical operators:

- `&&` logical AND
- `||` logical OR
- `!` logical NOT

It also supports the following word-based aliases:

- `and` for `&&`
- `or` for `||`
- `not` for `!`

The aliases use the same token types, precedence, truthiness rules, and
evaluation behavior as their symbolic equivalents. See
[`logical_keywords.md`](logical_keywords.md) for the implementation details.

## Syntax

```drift
var a = true && true
var b = true && false
var c = true || false
var d = !true
var e = !false

var f = true and true
var g = false or true
var h = not false
```

## Behavior

Logical operators evaluate to booleans. Numeric values are treated as truthy or falsy in the runtime. Strings with non-empty content are also considered truthy.

## Examples

```drift
say true && false
say true || false
say !false
say true and true
say false or true
say not false
```

## Output

```text
false
true
true
```

## Implementation notes

The logical evaluator checks the truthiness of each operand and then applies the correct boolean rule for `AND`, `OR`, or `NOT`.
