#include "elf_writer.h"
#include "vfs.h"
#include "common.h"
#include "heap.h"

#define ELF_FILE_OFFSET 0x1000u
#define ELF_BASE 0x40001000u
#define PT_LOAD 1u
#define PF_R 4u
#define PF_W 2u
#define PF_X 1u

typedef struct { u8 ident[16]; u16 type; u16 machine; u32 version; u32 entry; u32 phoff; u32 shoff; u32 flags; u16 ehsize; u16 phentsize; u16 phnum; u16 shentsize; u16 shnum; u16 shstrndx; } gc_ehdr_t;
typedef struct { u32 type; u32 offset; u32 vaddr; u32 paddr; u32 filesz; u32 memsz; u32 flags; u32 align; } gc_phdr_t;

int elf_write(const char *path, const u8 *code, u32 code_size, const u8 *data, u32 data_size, u32 entry_point) {
    gc_ehdr_t hdr = {0}; gc_phdr_t phdr = {0};
    u32 fd, i, total = code_size + data_size;
    u32 image_size = ELF_FILE_OFFSET + total;
    u8 *image = (u8 *)kmalloc(image_size);
    if (!image) return -1;
    memset(image, 0, image_size);
    hdr.ident[0] = 0x7f; hdr.ident[1] = 'E'; hdr.ident[2] = 'L'; hdr.ident[3] = 'F';
    hdr.ident[4] = 1; hdr.ident[5] = 1; hdr.ident[6] = 1;
    hdr.type = 2; hdr.machine = 3; hdr.version = 1; hdr.entry = entry_point;
    hdr.phoff = sizeof(hdr); hdr.ehsize = sizeof(hdr); hdr.phentsize = sizeof(phdr); hdr.phnum = 1;
    phdr.type = PT_LOAD; phdr.offset = ELF_FILE_OFFSET; phdr.vaddr = ELF_BASE; phdr.paddr = ELF_BASE;
    phdr.filesz = total; phdr.memsz = total; phdr.flags = PF_R | PF_W | PF_X; phdr.align = 0x1000u;
    memcpy(image, &hdr, sizeof(hdr));
    memcpy(image + sizeof(hdr), &phdr, sizeof(phdr));
    memcpy(image + ELF_FILE_OFFSET, code, code_size);
    memcpy(image + ELF_FILE_OFFSET + code_size, data, data_size);
    if (vfs_find(path)) vfs_unlink(path);
    if (!vfs_create(path, 1)) { kfree(image); return -1; }
    fd = vfs_open(path); if (fd == VFS_INVALID_FD) { kfree(image); return -1; }
    i = vfs_write(fd, image, image_size);
    vfs_close(fd); kfree(image);
    if (i != image_size) return -1;
    return 0;
}
