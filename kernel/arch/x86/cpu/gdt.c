/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

#include "gdt.h"
#include "common.h"
#include "tss.h"
#include "kprintf.h"

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  gran;
    u8  base_high;
} __attribute__((packed));

struct gdt_ptr {
    u16 limit;
    u64 base;
} __attribute__((packed));

static struct gdt_entry gdt[7];
static struct gdt_ptr gp;

extern void gdt_flush(uintptr_t);

static void gdt_set_gate(int num, uintptr_t base, u32 limit, u8 access, u8 gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_mid = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].gran = (limit >> 16) & 0x0F;
    gdt[num].gran |= gran & 0xF0;
    gdt[num].access = access;
}

void gdt_init(void) {
    kprintf("gdt_init: setting up gates...\n");
    gp.limit = (sizeof(struct gdt_entry) * 7) - 1;
    gp.base = (uintptr_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xAF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    /* User programs are ELF32 binaries; keep the kernel in 64-bit long mode
     * while entering user code through a 32-bit compatibility segment. */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    tss_init();
    uintptr_t tss_addr = (uintptr_t)&tss_entry;
    u32 tss_size = sizeof(tss_entry) - 1;

    gdt_set_gate(5, tss_addr, tss_size, 0x89, 0x00);

    /* Descriptor entry 6 holds bits 32..63 of TSS base address */
    u32 *high_desc = (u32 *)&gdt[6];
    high_desc[0] = (u32)(tss_addr >> 32);
    high_desc[1] = 0;

    kprintf("gdt_init: flushing GDT (gp.base=%016llX, limit=%u)...\n", (unsigned long long)gp.base, (unsigned)gp.limit);
    gdt_flush((uintptr_t)&gp);
    kprintf("gdt_init: loading TSS...\n");
    tss_load();
    kprintf("gdt_init: done!\n");
}