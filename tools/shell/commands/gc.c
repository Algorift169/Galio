#include "../../compiler/include/gc.h"
#include "kprintf.h"

int cmd_gc(int argc, char **argv) {
    int result;
    if (argc < 3) { kprintf("Usage: gc <source.c> <output>\n"); return -1; }
    kprintf("Compiling %s -> %s...\n", argv[1], argv[2]);
    result = gc_compile(argv[1], argv[2]);
    if (result == 0) kprintf("Compilation successful: %s\n", argv[2]);
    else kprintf("Compilation failed with error code %d\n", result);
    return result;
}
