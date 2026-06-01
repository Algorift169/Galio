#include "process/elf.h"
#include "paging.h"
#include "pmem.h"
#include "kprintf.h"
#include "common.h"
#include "process/process.h"

#define PAGE_SIZE 4096

static u8 checked_add_u32(u32 a, u32 b, u32 *out) {
    u32 value = a + b;
    if (value < a) {
        return 0;
    }
    *out = value;
    return 1;
}

static u8 elf_range_within_file(u32 offset, u32 length, u32 file_size) {
    u32 end;
    if (!checked_add_u32(offset, length, &end)) {
        return 0;
    }
    return end <= file_size;
}

static u8 elf_user_range_valid(u32 vaddr, u32 memsz) {
    u32 end;
    if (memsz == 0 || !checked_add_u32(vaddr, memsz - 1, &end)) {
        return 0;
    }
    if (vaddr < USER_HEAP_START || end > USER_SPACE_END) {
        return 0;
    }
    return 1;
}

static u8 elf_segment_overlaps(void *elf_data, elf_program_header_t *segment, u32 segment_index) {
    u32 seg_start = segment->p_vaddr;
    u32 seg_end = segment->p_vaddr + segment->p_memsz - 1;

    elf_header_t *hdr = (elf_header_t *)elf_data;
    for (u32 i = 0; i < segment_index; i++) {
        elf_program_header_t *other = (elf_program_header_t *)((u32)elf_data + hdr->e_phoff + i * hdr->e_phentsize);
        if (other->p_type != PT_LOAD || other->p_memsz == 0) {
            continue;
        }
        u32 other_end = other->p_vaddr + other->p_memsz - 1;
        if (seg_start <= other_end && other->p_vaddr <= seg_end) {
            return 1;
        }
    }
    return 0;
}

u32 elf_load(void *elf_data, u32 elf_size) {
    elf_header_t *hdr;
    elf_program_header_t *ph;
    page_directory_t *pd;
    u32 i, j, page;
    u32 vaddr, memsz, filesz, offset;
    u32 start_page, end_page, num_pages;
    u32 virt, phys;
    u8 *src, *dst;

    if (!elf_data || elf_size < sizeof(elf_header_t)) {
        kprintf("elf_load: ELF image too small\n");
        return 0;
    }

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
    if (hdr->ei_data != 1 || hdr->ei_version != 1 || hdr->e_version != 1) {
        kprintf("elf_load: Unsupported ELF encoding or version\n");
        return 0;
    }
    if (hdr->e_machine != 3) {
        kprintf("elf_load: Unsupported machine type %u\n", hdr->e_machine);
        return 0;
    }
    if (hdr->e_ehsize != sizeof(elf_header_t) || hdr->e_phentsize != sizeof(elf_program_header_t)) {
        kprintf("elf_load: Invalid ELF header sizes\n");
        return 0;
    }
    if (!elf_user_range_valid(hdr->e_entry, 1)) {
        kprintf("elf_load: Entry point outside user space: 0x%x\n", hdr->e_entry);
        return 0;
    }
    if (!elf_range_within_file(hdr->e_phoff, hdr->e_phnum * hdr->e_phentsize, elf_size)) {
        kprintf("elf_load: Program header table outside ELF image\n");
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

        if (filesz > memsz) {
            kprintf("elf_load: Segment %u has filesz > memsz\n", i);
            return 0;
        }
        if (!elf_range_within_file(offset, filesz, elf_size)) {
            kprintf("elf_load: Segment %u file range outside ELF image\n", i);
            return 0;
        }
        if (!elf_user_range_valid(vaddr, memsz)) {
            kprintf("elf_load: Segment %u maps outside user space\n", i);
            return 0;
        }
        if (elf_segment_overlaps(elf_data, ph, i)) {
            kprintf("elf_load: Segment %u overlaps an earlier load segment\n", i);
            return 0;
        }

        /* Map the entire segment (round to page boundaries) */
        start_page = vaddr & ~(PAGE_SIZE - 1);
        u32 rounded_size;
        if (!checked_add_u32(memsz, PAGE_SIZE - 1, &rounded_size) ||
            !checked_add_u32(vaddr, rounded_size, &end_page)) {
            kprintf("elf_load: Segment %u page range overflow\n", i);
            return 0;
        }
        end_page &= ~(PAGE_SIZE - 1);
        num_pages = (end_page - start_page) / PAGE_SIZE;

        kprintf("    Mapping %u pages from 0x%x to 0x%x\n", num_pages, start_page, end_page);

        /* Allocate and map each page */
        for (page = 0; page < num_pages; page++) {
            virt = start_page + page * PAGE_SIZE;
            phys = pmem_alloc(1);
            if (!phys) {
                kprintf("elf_load: Failed to allocate physical frame for virt=0x%x (page %u of %u)\n", virt, page, num_pages);
                return 0;
            }
            kprintf("      Allocated phys=0x%x for virt=0x%x (page %u)\n", phys, virt, page);
            /* Map with user access (PAGE_USER) for ELF */
            paging_map(pd, virt, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
            /* Zero the page */
            for (j = 0; j < PAGE_SIZE; j++) {
                ((u8 *)virt)[j] = 0;
            }
            kprintf("      Zeroed virt=0x%x\n", virt);
        }

        /* Copy file contents to memory */
        src = (u8 *)elf_data + offset;
        dst = (u8 *)vaddr;
        kprintf("    Copying %u bytes to vaddr=0x%x (offset=0x%x)\n", filesz, vaddr, offset);
        for (j = 0; j < filesz; j++) {
            dst[j] = src[j];
        }
        kprintf("    Copied %u bytes to vaddr=0x%x\n", filesz, vaddr);
    }

    kprintf("ELF load completed successfully\n");
    return hdr->e_entry;
}
