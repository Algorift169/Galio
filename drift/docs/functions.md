# Functions in Drift

Functions are declared with `fun`, a name, comma-separated parameters, and a
colon-terminated body. The body ends with `end`.

```drift
fun greet(name):
    say name
end

greet("Drift")
```

Arguments are evaluated at call time and bound to parameters in the function's
local environment. `return` evaluates an expression and exits the current
function call:

```drift
fun double(value):
    return value * 2
end
```

Function calls currently execute as statements. A call must provide exactly the
number of arguments declared by the function.
