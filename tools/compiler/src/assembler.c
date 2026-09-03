#include "assembler.h"
#include "heap.h"
#include "string.h"

static u32 number(const char *text) {
    u32 value = 0, base = 10; u8 negative = 0;
    while (*text == ' ' || *text == '\t') text++;
    if (*text == '-') { negative = 1; text++; }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) { base = 16; text += 2; }
    while (*text) { int digit = (*text >= '0' && *text <= '9') ? *text - '0' : (*text >= 'a' && *text <= 'f') ? *text - 'a' + 10 : (*text >= 'A' && *text <= 'F') ? *text - 'A' + 10 : -1; if (digit < 0 || (u32)digit >= base) break; value = value * base + (u32)digit; text++; }
    return negative ? (u32)(-(i32)value) : value;
}
static void copy_name(char *dst, const char *src, u32 length) { memcpy(dst, src, length); dst[length] = 0; }
static u8 same(const char *a, const char *b) { return strcmp(a, b) == 0; }
static void trim(char *text) { u32 n = strlen(text); while (n && (text[n - 1] == ' ' || text[n - 1] == '\t' || text[n - 1] == '\r')) text[--n] = 0; }
static char *operand(char *line, u32 index) { u32 commas = 0; while (*line) { if (*line == ',') commas++; if (commas == index) return line + 1; line++; } return line; }
static void split(char *line, char *mnemonic, char *op1, char *op2) { char *p = line, *comma; while (*p == ' ' || *p == '\t') p++; while (*p && *p != ' ' && *p != '\t') p++; copy_name(mnemonic, line + (u32)((line[0] == ' ' || line[0] == '\t') ? 1 : 0), (u32)(p - (line + (u32)((line[0] == ' ' || line[0] == '\t') ? 1 : 0)))); while (*p == ' ' || *p == '\t') p++; comma = p; while (*comma && *comma != ',') comma++; if (*comma) { copy_name(op1, p, (u32)(comma - p)); p = comma + 1; while (*p == ' ' || *p == '\t') p++; copy_name(op2, p, strlen(p)); } else { copy_name(op1, p, strlen(p)); op2[0] = 0; } trim(mnemonic); trim(op1); trim(op2); }
static u8 reg_code(const char *reg) { if (same(reg, "eax")) return 0; if (same(reg, "ecx")) return 1; if (same(reg, "edx")) return 2; if (same(reg, "ebx")) return 3; if (same(reg, "esp")) return 4; if (same(reg, "ebp")) return 5; if (same(reg, "esi")) return 6; if (same(reg, "edi")) return 7; return 255; }
static u32 instruction_size(char *line) { char m[16], a[64], b[64]; split(line, m, a, b); if (same(m, "ret") || same(m, "cdq") || same(m, "neg") || same(m, "not") || same(m, "idiv") || same(m, "imul") || same(m, "inc") || same(m, "dec") || same(m, "setne")) return same(m, "setne") ? 3 : 1 + (same(m, "ret") || same(m, "cdq") ? 0 : 1); if (same(m, "movzx")) return 3; if (same(m, "int")) return 2; if (same(m, "push") || same(m, "pop")) return 1 + (same(m, "push") && reg_code(a) == 255 ? 4 : 0); if (same(m, "jmp") || m[0] == 'j' || same(m, "call")) return same(m, "jmp") || same(m, "call") ? 5 : 6; if (same(m, "mov") && a[0] == '[') return 2; if (same(m, "mov") && reg_code(a) != 255 && b[0] != '[' && reg_code(b) == 255) return 5; if (same(m, "mov") && reg_code(a) != 255 && reg_code(b) != 255) return 2; if (same(m, "lea")) return 3; if (same(m, "mov")) return 6; if ((same(m, "add") || same(m, "sub")) && same(a, "esp")) return 3; if (same(m, "imul") && reg_code(a) == 0 && b[0] != 0) return 6; if (same(m, "add") || same(m, "sub") || same(m, "cmp")) return reg_code(b) == 255 ? 5 : 2; if (same(m, "test") || same(m, "xor") || same(m, "and") || same(m, "or")) return 2; return 0; }

void labels_init(label_table_t *labels) { memset(labels, 0, sizeof(*labels)); }
void labels_add(label_table_t *labels, const char *name, u32 address) { label_entry_t *entry; if (labels->count == labels->capacity) { u32 capacity = labels->capacity ? labels->capacity * 2 : 32; entry = (label_entry_t *)kmalloc(capacity * sizeof(*entry)); if (!entry) return; if (labels->entries) { memcpy(entry, labels->entries, labels->count * sizeof(*entry)); kfree(labels->entries); } labels->entries = entry; labels->capacity = capacity; } entry = &labels->entries[labels->count++]; entry->name = (char *)kmalloc(strlen(name) + 1); if (entry->name) strcpy(entry->name, name); entry->address = address; }
u32 labels_find(label_table_t *labels, const char *name) { u32 i; for (i = 0; i < labels->count; i++) if (labels->entries[i].name && same(labels->entries[i].name, name)) return labels->entries[i].address; return 0xffffffffu; }
static void emit8(u8 *out, u32 *at, u8 value) { out[(*at)++] = value; }
static void emit32(u8 *out, u32 *at, u32 value) { emit8(out, at, value); emit8(out, at, value >> 8); emit8(out, at, value >> 16); emit8(out, at, value >> 24); }
static void emit_modrm(u8 *out, u32 *at, u8 reg, u8 rm) { emit8(out, at, (u8)(0xc0 | (reg << 3) | rm)); }
static u32 target(label_table_t *labels, const char *text) { if ((*text >= '0' && *text <= '9') || *text == '-') return number(text); return labels_find(labels, text); }
static int encode(char *line, u8 *out, u32 *at, label_table_t *labels) {
    char m[16], a[64], b[64]; u8 ra, rb; u32 destination; split(line, m, a, b); ra = reg_code(a); rb = reg_code(b);
    if (same(m, "db")) { emit8(out, at, (u8)number(a)); return 0; }
    if (same(m, "ret")) { emit8(out, at, 0xc3); return 0; }
    if (same(m, "cdq")) { emit8(out, at, 0x99); return 0; }
    if (same(m, "int")) { emit8(out, at, 0xcd); emit8(out, at, (u8)number(a)); return 0; }
    if (same(m, "push") && ra != 255) { emit8(out, at, 0x50 + ra); return 0; }
    if (same(m, "pop") && ra != 255) { emit8(out, at, 0x58 + ra); return 0; }
    if (same(m, "push")) { emit8(out, at, 0x68); emit32(out, at, number(a)); return 0; }
    if (same(m, "mov") && ra != 255 && rb == 255 && b[0] != 0 && b[0] != '[') { emit8(out, at, 0xb8 + ra); emit32(out, at, target(labels, b)); return 0; }
    if (same(m, "mov") && a[0] == '[' && !strncmp(a, "[ebp", 4) && same(b, "eax")) { i32 off=(i32)number(a+4); emit8(out,at,0x89); if(off>=-128&&off<=127){emit8(out,at,0x45);emit8(out,at,(u8)off);}else{emit8(out,at,0x85);emit32(out,at,(u32)off);} return 0; }
    if (same(m, "mov") && ra == 0 && a[0] == 'e' && b[0] == '[' && !strncmp(b, "[ebp", 4)) { i32 off=(i32)number(b+4); emit8(out,at,0x8b); if(off>=-128&&off<=127){emit8(out,at,0x45);emit8(out,at,(u8)off);}else{emit8(out,at,0x85);emit32(out,at,(u32)off);} return 0; }
    if (same(m, "mov") && ra == 0 && same(b, "[eax]")) { emit8(out, at, 0x8b); emit8(out, at, 0x00); return 0; }
    if (same(m, "mov") && same(a, "[ebx]") && ra == 0) { emit8(out, at, 0x89); emit8(out, at, 0x03); return 0; }
    if (same(m, "lea") && ra == 0 && a[0] == '[' && !strncmp(a, "[ebp", 4)) { i32 off=(i32)number(a+4); emit8(out,at,0x8d); if(off>=-128&&off<=127){emit8(out,at,0x45);emit8(out,at,(u8)off);}else{emit8(out,at,0x85);emit32(out,at,(u32)off);} return 0; }
    if (same(m, "mov") && ra != 255 && rb != 255) { emit8(out, at, 0x89); emit_modrm(out, at, ra, rb); return 0; }
    if (same(m, "test") && ra == 0 && rb == 0) { emit8(out, at, 0x85); emit8(out, at, 0xc0); return 0; }
    if (same(m, "inc") && ra == 0) { emit8(out, at, 0x40); return 0; }
    if (same(m, "dec") && ra == 0) { emit8(out, at, 0x48); return 0; }
    if (same(m, "setne") && same(a, "al")) { emit8(out, at, 0x0f); emit8(out, at, 0x95); emit8(out, at, 0xc0); return 0; }
    if (same(m, "movzx") && same(a, "eax") && same(b, "al")) { emit8(out, at, 0x0f); emit8(out, at, 0xb6); emit8(out, at, 0xc0); return 0; }
    if (same(m, "imul") && ra == 0 && b[0] != 0) { emit8(out, at, 0x69); emit8(out, at, 0xc0); emit32(out, at, number(b)); return 0; }
    if ((same(m, "add") || same(m, "sub")) && same(a, "esp")) { emit8(out, at, 0x83); emit8(out, at, same(m, "add") ? 0xc4 : 0xec); emit8(out, at, (u8)number(b)); return 0; }
    if ((same(m, "add") || same(m, "sub") || same(m, "cmp")) && ra == 0 && rb != 255) { emit8(out, at, same(m, "add") ? 0x01 : same(m, "sub") ? 0x29 : 0x39); emit_modrm(out, at, rb, 0); return 0; }
    if (same(m, "xor") && ra == 0 && rb == 3) { emit8(out, at, 0x31); emit8(out, at, 0xd8); return 0; }
    if (same(m, "imul") && ra == 3) { emit8(out, at, 0xf7); emit8(out, at, 0xeb); return 0; }
    if (same(m, "idiv") && ra == 3) { emit8(out, at, 0xf7); emit8(out, at, 0xfb); return 0; }
    if (same(m, "neg") && ra == 0) { emit8(out, at, 0xf7); emit8(out, at, 0xd8); return 0; }
    if (same(m, "not") && ra == 0) { emit8(out, at, 0xf7); emit8(out, at, 0xd0); return 0; }
    if (same(m, "call") || same(m, "jmp") || m[0] == 'j') { u32 size = same(m, "jmp") || same(m, "call") ? 5 : 6; destination = target(labels, a); if (same(m, "call")) emit8(out, at, 0xe8); else if (same(m, "jmp")) emit8(out, at, 0xe9); else { emit8(out, at, 0x0f); emit8(out, at, same(m, "je") ? 0x84 : same(m, "jne") ? 0x85 : same(m, "jl") ? 0x8c : same(m, "jg") ? 0x8f : same(m, "jle") ? 0x8e : 0x8d); } emit32(out, at, destination - (*at + (size == 5 ? 4 : 4))); return 0; }
    return -1;
}
int gc_assemble(const char *source, u8 **code, u32 *code_size, u8 **data, u32 *data_size, u32 *entry_offset) {
    label_table_t labels; u8 *out; u32 offset = 0, at = 0; const char *p = source; char line[256]; labels_init(&labels); out = (u8 *)kmalloc(65536); if (!out) return -9;
    while (*p) { u32 n = 0; while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++; if (*p) p++; line[n] = 0; trim(line); if (!line[0] || line[0] == ';' || line[0] == '.') continue; if (line[n - 1] == ':') { line[n - 1] = 0; labels_add(&labels, line, offset); } else { u32 size = instruction_size(line); if (!size) { kfree(out); return -5; } offset += size; } }
    p = source; while (*p) { u32 n = 0; while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++; if (*p) p++; line[n] = 0; trim(line); if (!line[0] || line[0] == ';' || line[0] == '.' || line[n - 1] == ':') continue; if (encode(line, out, &at, &labels) != 0) { kfree(out); return -5; } }
    *code = out; *code_size = at; *data = 0; *data_size = 0; *entry_offset = 0; return 0;
}
