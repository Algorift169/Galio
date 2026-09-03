#include "assembler.h"
#include "heap.h"
#include "string.h"
#include "kprintf.h"

static u32 gc_number(const char *s) { u32 n = 0; int base = 10; if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; base = 16; } while (*s) { int d = (*s >= '0' && *s <= '9') ? *s - '0' : (*s >= 'a' && *s <= 'f') ? *s - 'a' + 10 : (*s >= 'A' && *s <= 'F') ? *s - 'A' + 10 : -1; if (d < 0 || d >= base) break; n = n * base + (u32)d; s++; } return n; }
static void gc_emit32(u8 *out, u32 *at, u32 value) { out[(*at)++] = (u8)value; out[(*at)++] = (u8)(value >> 8); out[(*at)++] = (u8)(value >> 16); out[(*at)++] = (u8)(value >> 24); }

int gc_assemble(const char *source, u8 **code, u32 *code_size, u8 **data, u32 *data_size, u32 *entry_offset) {
    const char *p = source; u8 *out = (u8 *)kmalloc(4096); u32 at = 0; (void)data; (void)data_size;
    if (!out) return -9;
    *entry_offset = 0;
    while (*p) {
        char line[128]; u32 n = 0; while (*p && *p != '\n' && n < sizeof(line)-1) line[n++] = *p++; if (*p) p++; line[n] = 0;
        while (*line == ' ' || *line == '\t') memmove(line, line + 1, strlen(line));
        if (strncmp(line, "mov eax,", 8) == 0) { out[at++] = 0xb8; gc_emit32(out, &at, gc_number(line + 8)); }
        else if (strncmp(line, "mov ebx,", 8) == 0) { out[at++] = 0xbb; gc_emit32(out, &at, gc_number(line + 8)); }
        else if (strncmp(line, "int", 3) == 0) { out[at++] = 0xcd; out[at++] = (u8)gc_number(line + 3); }
        else if (strncmp(line, "xor ebx, ebx", 12) == 0) { out[at++] = 0x31; out[at++] = 0xdb; }
        else if (strncmp(line, "ret", 3) == 0) out[at++] = 0xc3;
        else if (line[0] && line[0] != '.' && line[strlen(line)-1] != ':') { kfree(out); return -5; }
    }
    *code = out; *code_size = at; return 0;
}
