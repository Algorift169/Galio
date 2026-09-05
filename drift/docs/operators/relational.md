# Relational Operators

Drift supports relational comparisons:

- `==` equality
- `!=` inequality
- `>` greater than
- `<` less than
- `>=` greater than or equal
- `<=` less than or equal

## Syntax

```drift
var a = 5 == 5
var b = 5 != 3
var c = 10 > 5
var d = 3 < 8
var e = 10 >= 10
var f = 5 <= 10
```

## Behavior

Relational operators return boolean values. If the operands are strings, equality and inequality compare the string payload directly.

## Examples

```drift
say 7 > 3
say 4 == 4
say "hello" != "world"
```

## Output

```text
true
true
true
```

## Implementation notes

The relational logic is kept in the dedicated relational operator module and is used as a boolean-producing operation in expression evaluation.
