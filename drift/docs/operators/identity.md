# Identity Operator

Drift uses `is` to test whether two values preserve the identity of the same
heap-backed object. It is different from `==`, which compares values.

```drift
var original = "hello"
var alias = original
var separate = "hello"

say original is alias
say original is separate
say original == separate
```

Output:

```text
true
false
true
```

## Implementation

The lexer recognizes the word `is` and emits `TOKEN_IS`. The expression
evaluator assigns it relational precedence and sends it to the central operator
dispatcher as `OPERATOR_IS`. The implementation in
`interpreter/operator_identity.c` returns `true` only when both operands have
the same value type and the same preserved identity marker.

String and array copies preserve their identity marker when variables are
stored or read from the environment. A separately created string or array gets
a different marker even if its contents are equal. Primitive values do not
represent heap-backed objects, so their identity comparison returns `false`.