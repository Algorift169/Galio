# Drift Operator System

This document summarizes the operator families implemented in Drift. The interpreter supports arithmetic, relational, logical, bitwise, assignment, increment/decrement, range, access, and membership operations.

## Supported categories

1. Arithmetic
   - `+`, `-`, `*`, `/`, `%`
2. Relational
   - `==`, `!=`, `>`, `<`, `>=`, `<=`
3. Logical
   - `&&`, `||`, `!`
4. Bitwise
   - `&`, `|`, `^`, `~`, `<<`, `>>`
5. Assignment
   - `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
6. Increment/Decrement
   - `++`, `--`
7. Range
   - `...`
8. Access
   - `[]`, `.`
9. Membership
   - `in`
10. Identity
   - `is`
11. Ternary
   - `? :`

## Evaluation model

Operators are resolved using the runtime evaluation pipeline:

```text
Token stream -> parser -> expression text -> operator dispatch -> value result
```

The runtime uses a central dispatcher to select the correct operator family and then evaluates the value pair. Most operators accept numeric, boolean, string, or array values depending on the category.

## Examples

```drift
var x = 1 + 2
say x

var ok = 5 > 3 && true
say ok

var bits = 8 << 1
say bits

var count = 10
count += 5
say count

var arr = [1, 2, 3]
say arr[1]

say 2 in arr
```

## Notes

- Arithmetic is numeric-oriented and will reject unsupported operand types.
- Relational operators compare values and return boolean results.
- Logical operators evaluate boolean-like conditions.
- Bitwise operations are integer-based.
- Assignment operators update the existing variable in place.
- Access and membership operators rely on array and collection semantics.
