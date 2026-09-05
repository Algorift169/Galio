# Logical Keyword Operators

Drift supports word-based aliases for the symbolic logical operators. The original
symbols remain available:

| Word form | Symbolic form | Meaning |
| --- | --- | --- |
| `and` | `&&` | Logical AND |
| `or` | `||` | Logical OR |
| `not` | `!` | Logical NOT |

## Syntax

```drift
var both_true = true and true
var either_true = false or true
var inverted = not false
```

The aliases can be used anywhere the symbolic operators can be used, including
expressions with variables, comparisons, and parentheses:

```drift
var ready = 5 > 3 and true
var allowed = false or (2 == 2)
var unavailable = not ready
```

## How the implementation works

The feature is implemented as a lexer-level alias. It reuses the existing token
and evaluator behavior instead of creating a second logical-operator system.

1. The lexer reads a sequence of letters as an identifier in `lexer/lexer.c`.
2. After the built-in language keywords are checked, the lexer calls
   `logical_keyword_token_type` from `lexer/logical_keywords.c`.
3. That function compares the identifier with `and`, `or`, and `not`.
4. Matching words are converted to the existing token types:
   - `and` becomes `TOKEN_AND_AND`.
   - `or` becomes `TOKEN_OR_OR`.
   - `not` becomes `TOKEN_BANG`.
5. The original word is retained as the token value for diagnostics and token
   ownership. Non-matching words remain `TOKEN_IDENTIFIER` values.
6. The evaluator receives exactly the same token types as it receives for
   `&&`, `||`, and `!`, so it uses the existing precedence, truthiness, and
   operator dispatch logic.

The mapping is declared in `include/drift/logical_keywords.h`. The new lexer
source file is included in the `SRCS` list in `Makefile`, which makes the helper
part of the normal interpreter build.

## Precedence and evaluation

`and` has the same precedence as `&&`, and `or` has the same precedence as `||`.
Both are binary operators. `not` has the same unary behavior and precedence as
`!`.

For example, `and` binds more tightly than `or`:

```drift
var result = false or true and false
say result
```

This is evaluated as `false or (true and false)`, producing:

```text
false
```

Logical results are booleans. Boolean values use their normal truth values, and
numeric values follow the interpreter's existing truthiness rules.

## Compatibility

Both forms can be used in the same program, and existing programs using `&&`,
`||`, or `!` continue to use the original token and evaluation path.

The behavior is covered by `tests/operators_logical_keywords_test.df`, while
`tests/operators_logical_test.df` continues to cover the symbolic forms.
