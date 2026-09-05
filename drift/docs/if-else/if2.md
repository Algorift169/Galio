# IF2: Unless and When Statements in Drift

This document describes the `unless` and `when` statements in Drift. They add
negative-condition syntax and value-based branching without changing the
existing `if`, `elif`, and `else` statements.

## 1. Unless

`unless` executes its body only when its condition is false:

```drift
unless user_logged_in:
    say "Please log in"
end
```

This is equivalent to:

```drift
if !user_logged_in:
    say "Please log in"
```

The condition uses Drift's normal truthiness rules. The body uses the same
colon-and-indentation block structure as other control-flow statements. The
`end` keyword is optional; dedentation marks the end of the body.

## 2. When

`when` evaluates one subject and compares it with ordered case values:

```drift
var day = 3

when day:
    1:
        say "Monday"
    2:
        say "Tuesday"
    3:
        say "Wednesday"
    else:
        say "Unknown day"
end
```

The matching case body is executed once. Case values are evaluated at runtime,
and the first equal case wins. When no case matches, the optional `else` body
runs. With no matching case and no `else`, the statement does nothing.

## 3. Block structure

The `:` after the condition or subject begins the block. Dedentation ends the
body, so `end` is optional for `if`, `else`, `unless`, and `when`:

```drift
when value:
    1:
        say "one"
    else:
        say "other"
```

An explicit `end` remains accepted for compatibility with existing loop and
conditional programs.

Both statements support normal nested statements, including loops and other
conditionals. Runtime errors and loop-control results from their bodies are
propagated to the enclosing statement.

## 4. Summary

Drift now supports:

- `unless condition:` for an inverted conditional
- `when subject:` for ordered value-based branching
- optional `else` handling for `when`
- runtime evaluation of conditions, subjects, and case values
- nested statement bodies using the existing block syntax
