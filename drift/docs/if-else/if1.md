# IF1: Conditional Statements in Drift

This document describes the conditional statement system in Drift, including the `if`, `elif`, and `else` branches used for selecting code paths based on runtime conditions.

The goal of this milestone is to add structured branching to the language while keeping the grammar simple and easy to parse.

## 1. Overview

Drift now supports conditional execution using this syntax:

```drift
if condition :
    say "condition is true"
else :
    say "condition is false"
```

The interpreter evaluates the condition expression and executes only the matching branch. If the first condition is false, the interpreter checks later `elif` branches and finally the optional `else` block.

---

## 2. Supported syntax

### Basic if/else

```drift
if x > 0 :
    say "positive"
else :
    say "not positive"
```

### If with multiple branches

```drift
if x > 0 :
    say "positive"
elif x == 0 :
    say "zero"
else :
    say "negative"
```

### Condition expression

The condition is evaluated as a standard Drift expression, so values such as comparisons and logical expressions can be used inside the condition:

```drift
if score >= 10 && lives > 0 :
    say "player is alive and winning"
else :
    say "player lost"
```

---

## 3. Branch behavior

The runtime follows this order:

1. evaluate the first `if` condition
2. if it is truthy, run that branch
3. otherwise move to the next `elif` condition
4. if every condition is false, execute the `else` block when present
5. if no branch matches, do nothing

The condition is considered truthy if the evaluated value is a boolean true or any value the runtime treats as logically true.

---

## 4. Examples

### Example 1: simple positive/negative check

```drift
var x = -2

if x > 0 :
    say "positive"
else :
    say "non-positive"
```

Output:

```text
non-positive
```

### Example 2: multiple choices

```drift
var grade = 85

if grade >= 90 :
    say "A"
elif grade >= 80 :
    say "B"
elif grade >= 70 :
    say "C"
else :
    say "F"
```

Output:

```text
B
```

### Example 3: boolean condition

```drift
var online = true

if online :
    say "connected"
else :
    say "offline"
```

Output:

```text
connected
```

---

## 5. Block structure

The `:` ends the condition and begins a block. Each branch body is a sequence of Drift statements.

Example:

```drift
if x == 1 :
    say "one"
    say "still one"
else :
    say "not one"
```

This means each branch can contain one or more statements, not just a single line.

---

## 6. Parsing model

The parser handles conditional statements as a structured block with multiple branches.

Internally, it builds a branch list like this:

- one `if` branch
- zero or more `elif` branches
- optional `else` body

Each branch stores:

- the condition text
- the body statements
- the number of statements in the body

The interpreter then evaluates each branch in order until one condition succeeds.

---

## 7. Error cases

### Missing condition

```drift
if :
    say "oops"
```

This is invalid because no condition expression appears after `if`.

### Missing colon

```drift
if x > 0
    say "oops"
```

This is invalid because the condition must be followed by `:`.

### Invalid condition expression

```drift
if 5 + :
    say "wrong"
```

This fails because the condition cannot be parsed into a valid Drift expression.

### Empty else block

```drift
if true :
    say "yes"
else :
```

This is invalid unless the else block contains at least one statement.

---

## 8. Current limitations

This beta implementation keeps the conditional system intentionally simple:

- no nested `if` blocks yet in the same branch body beyond normal statement parsing
- condition evaluation is still expression-based and depends on current runtime operator support
- no advanced ternary-style shorthand
- no additional block keywords beyond `if`, `elif`, and `else`

The core feature is present and functional: condition-based branching works as a first-class statement in Drift.

---

## 9. Summary

The language now supports structured branching with the following grammar:

```drift
if condition :
    ...
elif condition :
    ...
else :
    ...
```

This gives Drift a straightforward way to express decision logic without expanding the language beyond its current minimal design.
