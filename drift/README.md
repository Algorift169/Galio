# Drift

Drift is a small interpreted scripting language written in C. It follows a basic interpreter pipeline:

Source code -> Lexer -> Tokens -> Parser -> AST -> Interpreter -> Output

## Features

- `say` statements for output
- variable declarations with `var`
- string, integer, float, and boolean literals
- **conditional statements** with `if`, `elif`, and `else`
- **comments support** (`//` single-line and `/* */` multi-line)
- simple REPL mode with multi-line comment continuation
- file-based execution
- interpolation support for values like `say "value: {name}"`
- robust error handling that reports errors without crashing

## Build

From the project root:

```bash
make
```

This creates the binary in `./build/drift`.

To remove build artifacts:

```bash
make clean
```

## Run

### REPL

```bash
./build/drift
```

Then enter commands such as:

```text
>>> say "Hello, Drift!"
>>> var name = "Israfil"
>>> say "Hello, {name}!"
>>> // Comments work too!
>>> var x = 10  /* inline comment */
>>> exit
```

**Note:** If you open a block comment (`/*`), the REPL will show `...` as the prompt while waiting for you to close it:

```text
>>> var x = 10  /* open comment
... var y = 20
... */
>>>
```

### File execution

```bash
./build/drift examples/hello.drift
```

## Example programs

```drift
// Greeting program
say "Hello, World!"

/* User information section */
var name = "Israfil"
var age = 20

say "Name: {name}"      // Print name
say "Age: {age}"        /* Print age */
```

## Conditional Statements

Drift supports `if`, `elif`, and `else` statements for control flow:

```drift
var score = 85

if score >= 90 :
    say "Excellent!"
elif score >= 80 :
    say "Good job!"
elif score >= 70 :
    say "Passing"
else :
    say "Needs improvement"
```

**Note:** Conditional statements work correctly in **file mode**. The REPL has a known limitation with multi-line if/elif/else blocks due to its line-oriented nature. For interactive testing of conditionals, use the file-based execution.

## Notes

- The interpreter keeps a runtime environment for declared variables.
- Variable declarations are literal-based and intentionally simple.
- The project is organized into lexer, parser, token, interpreter, and runtime pieces.

## Project layout

```text
.
├── main.c
├── Makefile
├── include/
├── lexer/
│   ├── comments/
│   ├── core/
│   └── keywords/
├── parser/
│   ├── arrays/
│   ├── control_flow/
│   └── core/
├── token/
├── interpreter/
│   ├── arrays/
│   ├── core/
│   └── operators/
├── docs/
├── tests/
│   ├── arrays/
│   ├── basic/
│   ├── comments/
│   ├── errors/
│   ├── if-else/
│   ├── input/
│   ├── integration/
│   ├── loop/
│   ├── operators/
│   └── variables/
└── build/
```

Tests are grouped under `tests/` by language feature. Input tests may require
interactive input, and the files under `tests/errors/` intentionally contain
invalid programs.

## Documentation

See the docs folder for language notes and milestones:

- [docs/printing/p1.md](docs/printing/p1.md) - Print statements
- [docs/variables/v1.md](docs/variables/v1.md) - Variable declarations
- [docs/comment/c1.md](docs/comment/c1.md) - Comment syntax and features
- [docs/comment/c2.md](docs/comment/c2.md) - Comment examples and best practices
- [docs/comment/c3.md](docs/comment/c3.md) - Comment implementation details
