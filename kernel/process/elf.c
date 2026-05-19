#include "elf.h"
#include "paging.h"
#include "pmem.h"
#include "kprintf.h"
#include "common.h"

#define PAGE_SIZE 4096

u32 elf_load(void *elf_data) {
    elf_header_t *hdr;
    elf_program_header_t *ph;
    page_directory_t *pd;
    u32 i, j, page;
    u32 vaddr, memsz, filesz, offset;
    u32 start_page, end_page, num_pages;
    u32 virt, phys;
    u8 *src, *dst;

    hdr = (elf_header_t *)elf_data;

    /* Validate ELF header */
    if (hdr->magic != ELF_MAGIC) {
        kprintf("elf_load: Invalid ELF magic (got 0x%x, expected 0x%x)\n", hdr->magic, ELF_MAGIC);
        return 0;
    }

    if (hdr->ei_class != 1) {
        kprintf("elf_load: Not a 32-bit ELF (class=%d)\n", hdr->ei_class);
        return 0;
    }

    kprintf("ELF entry point: 0x%x\n", hdr->e_entry);
    kprintf("ELF program headers: %d (offset=0x%x, size=%d)\n", 
            hdr->e_phnum, hdr->e_phoff, hdr->e_phentsize);

    pd = paging_get_current();
    if (!pd) {
        kprintf("elf_load: No page directory\n");
        return 0;
    }

    /* Load each program header */
    for (i = 0; i < hdr->e_phnum; i++) {
        ph = (elf_program_header_t *)((u32)elf_data + hdr->e_phoff + i * hdr->e_phentsize);

        if (ph->p_type != PT_LOAD) {
            continue;
        }

        kprintf("  Segment %u: vaddr=0x%x, filesz=%u, memsz=%u, offset=0x%x\n",
                i, ph->p_vaddr, ph->p_filesz, ph->p_memsz, ph->p_offset);

        vaddr = ph->p_vaddr;
        memsz = ph->p_memsz;
        filesz = ph->p_filesz;
        offset = ph->p_offset;

        /* Map the entire segment (round to page boundaries) */
        start_page = vaddr & ~(PAGE_SIZE - 1);
        end_page = (vaddr + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        num_pages = (end_page - start_page) / PAGE_SIZE;

        kprintf("    Mapping %u pages from 0x%x to 0x%x\n", num_pages, start_page, end_page);

        /* Allocate and map each page */
        for (page = 0; page < num_pages; page++) {
            virt = start_page + page * PAGE_SIZE;
            phys = pmem_alloc(1);
            if (!phys) {
                kprintf("elf_load: Failed to allocate physical frame for virt=0x%x\n", virt);
                return 0;
            }
            /* Map with user access (PAGE_USER) for ELF */
            paging_map(pd, virt, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
            /* Zero the page */
            for (j = 0; j < PAGE_SIZE; j++) {
                ((u8 *)virt)[j] = 0;
            }
        }

        /* Copy file contents to memory */
        src = (u8 *)elf_data + offset;
        dst = (u8 *)vaddr;
        for (j = 0; j < filesz; j++) {
            dst[j] = src[j];
        }
    }

    kprintf("ELF load completed successfully\n");
    return hdr->e_entry;
}