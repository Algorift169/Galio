#include "../include/gc.h"
#include "codegen.h"
#include "elf_writer.h"
#include "vfs.h"
#include "heap.h"

int gc_compile(const char *source_path, const char *output_path) {
    u32 size = vfs_size(source_path); u8 *source; u8 *code = 0; u8 *data = 0; u32 code_size = 0, data_size = 0, entry = 0; int result;
    if (!size) return GC_ERR_FILE_OPEN;
    source = (u8 *)kmalloc(size + 1); if (!source) return GC_ERR_MEMORY;
    if (vfs_read(source_path, source, size) != size) { kfree(source); return GC_ERR_FILE_READ; }
    source[size] = 0; result = gc_codegen_source((const char *)source, &code, &code_size, &data, &data_size, &entry); kfree(source);
    if (result != 0) return result;
    result = elf_write(output_path, code, code_size, data, data_size, 0x40001000u + entry);
    kfree(code); kfree(data); return result == 0 ? GC_OK : GC_ERR_FILE_WRITE;
}
