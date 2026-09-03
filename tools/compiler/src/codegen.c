#include "codegen.h"
#include "heap.h"
#include "string.h"

static u32 find_string(const char *source, char *out, u32 max) {
    const char *p = source; u32 n = 0;
    while (*p && *p != '"') p++;
    if (!*p) return 0;
    p++;
    while (*p && *p != '"' && n + 1 < max) { if (*p == '\\' && p[1] == 'n') { out[n++] = '\n'; p += 2; } else { out[n++] = (u8)*p++; } }
    out[n] = 0; return n;
}
static int source_has(const char *source, const char *needle) {
    u32 needle_length = strlen(needle);
    while (*source) { if (strncmp(source, needle, needle_length) == 0) return 1; source++; }
    return 0;
}

int gc_codegen_source(const char *source, u8 **code, u32 *code_size, u8 **data, u32 *data_size, u32 *entry_offset) {
    char message[512]; u32 length = find_string(source, message, sizeof(message));
    u8 *out = (u8 *)kmalloc(64); u32 at = 0, data_address;
    if (!out || !data) return -9;
    if ((!source_has(source, "galio_write") && !source_has(source, "printf")) ||
        (!source_has(source, "galio_exit") && !source_has(source, "main")) || !length) {
        if (out) kfree(out);
        return -4;
    }
    out[at++] = 0xb8; out[at++] = 1; out[at++] = 0; out[at++] = 0; out[at++] = 0;
    out[at++] = 0xbb; out[at++] = 1; out[at++] = 0; out[at++] = 0; out[at++] = 0;
    out[at++] = 0xb9; data_address = 0; out[at++] = 0; out[at++] = 0; out[at++] = 0; out[at++] = 0;
    out[at++] = 0xba; out[at++] = (u8)length; out[at++] = (u8)(length >> 8); out[at++] = 0; out[at++] = 0;
    out[at++] = 0xcd; out[at++] = 0x80; out[at++] = 0xb8; out[at++] = 60; out[at++] = 0; out[at++] = 0; out[at++] = 0; out[at++] = 0x31; out[at++] = 0xdb; out[at++] = 0xcd; out[at++] = 0x80;
    data_address = 0x40001000u + at;
    out[11] = (u8)data_address; out[12] = (u8)(data_address >> 8);
    out[13] = (u8)(data_address >> 16); out[14] = (u8)(data_address >> 24);
    *data = (u8 *)kmalloc(length); if (!*data) { kfree(out); return -9; } memcpy(*data, message, length);
    *code = out; *code_size = at; *data_size = length; *entry_offset = 0; return 0;
}
