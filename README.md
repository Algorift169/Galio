# Galio

Galio is a custom x86_64 kernel and shell environment focused on a compact OS core, privilege-aware filesystem access, syscall experimentation, and power lifecycle hooks.

The project is currently built as a freestanding x86_64 kernel with a custom GRUB boot flow, a VFS-backed shell environment, and a Linux-inspired compatibility layer adapted to Galio naming and kernel conventions.

---

## Highlights

- x86_64 long-mode kernel build
- custom boot and linker setup
- VFS and EXT2-style filesystem layer
- interactive shell with privileged rex flow
- reusable GSH option parsing for declared command options
- syscall compatibility surface for common kernel calls
- root-only file and directory protection through rex
- power subsystem scaffolding for reset, shutdown, and suspend
- desktop-style shell utilities and process view

## Filesystem model

Galio keeps a familiar Unix-shaped directory layout where it helps users and
tools, but the filesystem is Galio's VFS rather than a Linux userland clone.
The supported persistent filesystem is the VFS initrd plus the EXT2-backed
disk store. The usual objects are represented uniformly as filesystem entries:
regular files, directories, symbolic links, and registered device files.
Character devices such as `./dev/console`, `./dev/null`, `./dev/keyboard`, and
`./dev/mouse` are opened, read, and written through the same file-descriptor
path as regular files.

Galio does not create unsupported Linux compatibility surfaces such as `/proc`,
`/sys`, `/run` service state, `/etc/fstab`, foreign shell profiles, SSH/cron/udev
configuration, fake init scripts, or placeholder shell binaries. Runtime
information belongs in real kernel APIs and registered device files; a path is
not advertised merely because Linux uses that name.

---

## Current build

Requirements are the standard host toolchain for a bare-metal x86_64 kernel:

```bash
sudo apt update
sudo apt install -y build-essential nasm binutils xorriso grub-pc-bin mtools qemu-system-x86
```

Build and run:

```bash
make clean && make -j$(nproc)
./scripts/run.sh
```

Optional fullscreen:

```bash
./scripts/run_fullscreen.sh
```

---

## Main areas

- kernel/ — kernel entry, runtime, tests, and subsystem logic
- include/ — public headers for memory, process, fs, syscall, and power
- init/ — early init flow
- tools/shell/ — interactive shell and privileged command layer
- tools/shell/commands/ — file, dir, write, delete, top, syscall, and other commands
- kernel/power/ — reboot, shutdown, and suspend scaffolding
- kernel/syscall/ — syscall dispatch and compatibility handlers
- ui/ — display/panel primitives for the shell UI

---

## Shell behavior

The shell includes a privileged rex flow that requires a password once per session. Root-level directory and file modifications are intentionally blocked unless they are done through rex.

GSH command options use bounded, command-declared parsing. Options must appear before positional arguments; short options may be combined, long options support `--name=value` and required values, and `--` terminates option parsing. A single `-` remains a positional argument.

Scripts use the `.gh` extension and can run every existing shell command through `run <file>` or `source <file>`. A `.gh` file can also be executed directly, for example `./file.gh`. One statement is accepted per line. Variables use `let` or `set`, expressions use C-style scalar operators, and control blocks use `if`/`else`/`endif` and `while`/`endwhile`:

Drift syntax is also accepted in `.gh` files: `var`, `say`, `elif`, `unless`, `when`, `repeat`, `for`, `each`, `fun`, `return`, `break`, `continue`, `end`, `and`, `or`, `not`, array literals, array indexing, ranges, and `//` or `/* ... */` comments. Variable declarations may contain multiple bindings:

```drift
var i = 0, j = 1
var name = "Galio", active = true
var values[] = {10, 20, 30}
```

Expression statements also work directly at the prompt or through `gsh`:

```text
var x = 10
x++
x--
++x
x += 2
say x
```

Inline Drift loops are also accepted at the prompt:

```text
for(var i = 0, i < 10, i++): say "hi"
```

```text
let count = 1
while count <= 3
	echo item-$count
	count++
endwhile
if count == 4
	echo complete
else
	echo failed
endif
```

Examples:

```bash
rex goto .
rex back
rex file example.txt
rex dir new_folder
rex syscall mmap 400
```

---

## Notes

- This project is not a Linux kernel clone; it borrows Linux-style structure and ideas where useful, but keeps Galio-specific naming and runtime behavior.
- Some syscall and power paths are compatibility stubs intended for kernel development and testing rather than full hardware-level completion.
- Current work is focused on the kernel shell, privilege model, VFS operations, power lifecycle hooks, and syscall compatibility.

---

## Development status

- x86_64 boot and kernel startup: operational through GRUB Multiboot2
- Shell and privilege layer: operational, with `gsh` as the native scripting language
- Filesystem and process layer: operational VFS/EXT2 path with file-backed device nodes
- Native syscall path: x86_64 `SYSCALL/SYSRET` operational with INT 0x80 compatibility
- Storage path: ATA fallback operational; NVMe/MSI-X support integrated and under hardware validation
- Power lifecycle support: kernel interfaces and platform scaffolding available

This README reflects the current x86_64 project state, including the Multiboot2 boot contract,
Galio's file-centric VFS model, and `gsh` rather than a Linux shell or userland layout.
