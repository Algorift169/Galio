# Ternary Operator

Drift supports the familiar ternary operator for choosing one of two values:

```drift
condition ? value_if_true : value_if_false
```

## Examples

```drift
var age = 20
var status = age >= 18 ? "Adult" : "Minor"
say status
```

Output:

```text
Adult
```

A ternary expression can also select between variables:

```drift
var a = 10
var b = 20
var max = a > b ? a : b
say max
```

Output:

```text
20
```

## Behavior

- The condition uses Drift truthiness rules.
- A true condition returns `value_if_true`.
- A false condition returns `value_if_false`.
- Ternary expressions have lower precedence than the other binary operators.
- Ternary expressions are right-associative, so nested expressions are evaluated from the false branch:

```drift
var result = false ? "outer" : true ? "inner" : "fallback"
say result
```

Output:

```text
inner
```

## Implementation notes

The implementation follows the normal Drift expression pipeline:

```text
source text -> lexer tokens -> precedence-climbing evaluator -> ternary operator -> Value
```

### Lexing

The lexer creates two separate tokens for the ternary delimiters:

```text
? -> TOKEN_QUESTION
: -> TOKEN_COLON
```

The colon token is also used by `if` statements, so the lexer only identifies the character. The expression evaluator decides whether the colon closes a ternary expression.

### Expression algorithm

Drift evaluates expressions with a precedence-climbing algorithm. Each recursive call receives a minimum precedence and consumes operators while their precedence is high enough.

For a ternary expression, the evaluator performs these steps:

1. Evaluate the expression on the left of `?`. This is the condition.
2. Consume `TOKEN_QUESTION`.
3. Recursively evaluate the true branch until `TOKEN_COLON` is reached.
4. Require and consume `TOKEN_COLON`.
5. Recursively evaluate the false branch using the ternary precedence again.
6. Pass the condition and both branch values to `operator_apply_ternary`.
7. Replace the condition with a copy of the selected branch value.

The relevant algorithm can be summarized as:

```text
left = evaluate_expression(min_precedence)

if next_token == '?':
	consume '?'
	true_value = evaluate_expression(0) until ':'
	require and consume ':'
	false_value = evaluate_expression(ternary_precedence)
	left = select(left, true_value, false_value)
```

The false branch uses the same precedence instead of a higher precedence. This makes the operator right-associative, so:

```drift
a ? b : c ? d : e
```

is interpreted as:

```text
a ? b : (c ? d : e)
```

Ternary precedence is lower than the other binary operators, so:

```drift
age >= 18 ? "Adult" : "Minor"
```

evaluates the comparison before choosing a branch.

### Value selection

The dedicated implementation in `interpreter/operators/operator_ternary.c` applies Drift truthiness rules. Boolean values use their boolean value; integers and floats are false when zero; empty strings and null are false; non-empty strings, arrays with a value, and infinity are true. It returns a copy of the selected branch so the result owns its value independently.

Both branch expressions are evaluated before selection in the current implementation. Ternary evaluation therefore selects the result value, but does not yet short-circuit runtime evaluation of the unused branch.
