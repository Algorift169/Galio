#include "arch/x86/apic.h"
#include "mm/paging.h"
#include "lib/kprintf.h"
#include "lib/string.h"

#define APIC_REG_ID       0x020
#define APIC_REG_EOI      0x0B0
#define APIC_REG_SVR      0x0F0
#define IOAPIC_REGSEL     0x00
#define IOAPIC_WIN        0x10
#define IOAPIC_VER        0x01
#define IOAPIC_REDTBL     0x10

typedef struct {
    u32 gsi;
    u16 flags;
    u8 present;
} irq_override_t;

static volatile u32 *lapic_base;
static volatile u32 *ioapic_base;
static u32 ioapic_gsi_base;
static u32 ioapic_redirections;
static u8 apic_ready;
static u8 lapic_id;
static irq_override_t irq_overrides[16];

static u8 acpi_checksum(const u8 *data, u32 length) {
    u8 sum = 0;
    for (u32 i = 0; i < length; i++) sum = (u8)(sum + data[i]);
    return sum == 0;
}

static u8 acpi_signature(const u8 *data, const char *signature) {
    for (u32 i = 0; i < 8; i++) {
        if (data[i] != (u8)signature[i]) return 0;
    }
    return 1;
}

static u8 *acpi_find_rsdp(void) {
    u16 ebda_segment = *(volatile u16 *)(uintptr_t)0x40E;
    u32 ebda = (u32)ebda_segment << 4;
    u8 *candidate;

    if (ebda >= 0x400 && ebda < 0xA0000) {
        for (candidate = (u8 *)(uintptr_t)ebda;
             candidate < (u8 *)(uintptr_t)(ebda + 1024); candidate += 16) {
            if (acpi_signature(candidate, "RSD PTR ") &&
                acpi_checksum(candidate, 20)) return candidate;
        }
    }
    for (candidate = (u8 *)(uintptr_t)0xE0000;
         candidate < (u8 *)(uintptr_t)0x100000; candidate += 16) {
        if (acpi_signature(candidate, "RSD PTR ") &&
            acpi_checksum(candidate, 20)) return candidate;
    }
    return NULL;
}

static u8 acpi_get_root(const u8 *rsdp, u8 **root) {
    u64 address;
    u32 length = 20;

    if (rsdp[15] >= 2) {
        length = *(const u32 *)(rsdp + 20);
        if (length < 36 || !acpi_checksum(rsdp, length)) return 0;
        address = *(const u64 *)(rsdp + 24);
    } else {
        if (!acpi_checksum(rsdp, 20)) return 0;
        address = *(const u32 *)(rsdp + 16);
    }
    if (address > 0xFFFFFFFFull) return 0;
    *root = (u8 *)(uintptr_t)address;
    return 1;
}

static void ioapic_write(u8 reg, u32 value) {
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    ioapic_base[IOAPIC_WIN / 4] = value;
}

static u32 ioapic_read(u8 reg) {
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    return ioapic_base[IOAPIC_WIN / 4];
}

static void ioapic_route(u8 irq, u8 vector) {
    u32 gsi = irq;
    u32 flags = 0;
    u32 redirection;

    if (irq < 16 && irq_overrides[irq].present) {
        gsi = irq_overrides[irq].gsi;
        if ((irq_overrides[irq].flags & 3u) == 3u) flags |= 1u << 13;
        if ((irq_overrides[irq].flags & 12u) == 12u) flags |= 1u << 15;
    }
    if (!ioapic_base || gsi < ioapic_gsi_base ||
        gsi - ioapic_gsi_base >= ioapic_redirections) return;

    redirection = IOAPIC_REDTBL + (gsi - ioapic_gsi_base) * 2u;
    ioapic_write((u8)redirection, vector | flags);
    ioapic_write((u8)(redirection + 1u), (u32)lapic_id << 24);
}

static void apic_parse_madt(u8 *madt) {
    u32 length = *(u32 *)(madt + 4);
    u32 offset = 44;
    u32 lapic_phys;

    if (length < offset) return;
    lapic_phys = *(u32 *)(madt + 36);
    lapic_base = (volatile u32 *)mmio_map_physical(lapic_phys, PAGE_SIZE);
    while (offset + 2 <= length) {
        u8 type = madt[offset];
        u8 entry_length = madt[offset + 1];
        if (entry_length < 2 || offset + entry_length > length) break;
        if (type == 1 && entry_length >= 12 && !ioapic_base) {
            u32 phys = *(u32 *)(madt + offset + 4);
            ioapic_gsi_base = *(u32 *)(madt + offset + 8);
            ioapic_base = (volatile u32 *)mmio_map_physical(phys, PAGE_SIZE);
        } else if (type == 2 && entry_length >= 10) {
            u8 source = madt[offset + 3];
            if (source < 16) {
                irq_overrides[source].gsi = *(u32 *)(madt + offset + 4);
                irq_overrides[source].flags = *(u16 *)(madt + offset + 8);
                irq_overrides[source].present = 1;
            }
        }
        offset += entry_length;
    }
}

void apic_init(void) {
    u8 *rsdp;
    u8 *root;
    u32 root_length;
    u32 entry_size;
    u32 entries;

    memset(irq_overrides, 0, sizeof(irq_overrides));
    rsdp = acpi_find_rsdp();
    if (!rsdp || !acpi_get_root(rsdp, &root)) {
        kprintf("APIC: ACPI unavailable; using legacy PIC\n");
        return;
    }
    if (!acpi_signature(root, "RSDT    ") && !acpi_signature(root, "XSDT    ")) {
        kprintf("APIC: invalid ACPI root table; using legacy PIC\n");
        return;
    }
    root_length = *(u32 *)(root + 4);
    if (root_length < 36 || !acpi_checksum(root, root_length)) return;
    entry_size = acpi_signature(root, "XSDT    ") ? 8u : 4u;
    entries = (root_length - 36u) / entry_size;
    for (u32 i = 0; i < entries; i++) {
        u64 address = entry_size == 8u ? *(u64 *)(root + 36u + i * 8u) :
                      *(u32 *)(root + 36u + i * 4u);
        u8 *table;
        if (address > 0xFFFFFFFFull) continue;
        table = (u8 *)(uintptr_t)address;
        if (acpi_signature(table, "APIC    ")) {
            apic_parse_madt(table);
            break;
        }
    }
    if (!lapic_base || !ioapic_base) {
        kprintf("APIC: MADT has no usable APICs; using legacy PIC\n");
        return;
    }
    lapic_id = (u8)(lapic_base[APIC_REG_ID / 4] >> 24);
    lapic_base[APIC_REG_SVR / 4] |= 0x100u;
    ioapic_redirections = ((ioapic_read(IOAPIC_VER) >> 16) & 0xFFu) + 1u;
    apic_ready = 1;
    kprintf("APIC: enabled local APIC %u with %u IOAPIC entries\n",
            lapic_id, ioapic_redirections);
}

u8 apic_is_available(void) { return apic_ready; }
void apic_eoi(void) { if (lapic_base) lapic_base[APIC_REG_EOI / 4] = 0; }
u32 apic_cpu_id(void) { return lapic_id; }

void apic_register_irq(u8 irq, interrupt_handler_t handler) {
    u32 vector = irq < 16u ? 32u + irq : 48u + ((u32)irq - 16u);
    if (!handler || vector >= 64u) return;
    interrupt_install_handler(vector, handler);
    ioapic_route(irq, (u8)vector);
}