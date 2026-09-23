#include "arch/x86/apic.h"
#include "mm/paging.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "gdt.h"
#include "idt.h"

#define APIC_REG_ID       0x020
#define APIC_REG_EOI      0x0B0
#define APIC_REG_SVR      0x0F0
#define IOAPIC_REGSEL     0x00
#define IOAPIC_WIN        0x10
#define IOAPIC_VER        0x01
#define IOAPIC_REDTBL     0x10
#define APIC_REG_ICR_LOW  0x300
#define APIC_REG_ICR_HIGH 0x310
#define APIC_ICR_DELIVERY 0x1000u
#define APIC_ICR_DEASSERT 0x0500u
#define APIC_ICR_INIT     0x4500u
#define APIC_ICR_STARTUP  0x4600u
#define AP_TRAMPOLINE_PHYS 0x8000u
#define AP_PARAMS_PHYS     0x9000u
#define AP_MAX_CPUS        16u
#define AP_STACK_SIZE      16384u

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
static u8 cpu_ids[AP_MAX_CPUS];
static u32 cpu_count;
static volatile u32 online_cpu_count;
static u8 ap_stacks[AP_MAX_CPUS][AP_STACK_SIZE] __attribute__((aligned(16)));

extern u8 ap_trampoline_start;
extern u8 ap_trampoline_end;
extern u8 ap_protected_mode;
extern u8 ap_long_mode;
extern void ap_entry(void);

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
        if (type == 0 && entry_length >= 8u && (madt[offset + 4u] & 1u) &&
            cpu_count < AP_MAX_CPUS) {
            cpu_ids[cpu_count++] = madt[offset + 3u];
        } else if (type == 9 && entry_length >= 16u &&
                   (*(u32 *)(madt + offset + 8u) & 1u) &&
                   *(u32 *)(madt + offset + 4u) <= 0xFFu &&
                   cpu_count < AP_MAX_CPUS) {
            cpu_ids[cpu_count++] = (u8)*(u32 *)(madt + offset + 4u);
        } else if (type == 1 && entry_length >= 12 && !ioapic_base) {
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
    apic_ready = 0u;
    lapic_base = NULL;
    ioapic_base = NULL;
    cpu_count = 0u;
    online_cpu_count = 0u;
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
    online_cpu_count = 1u;
    lapic_base[APIC_REG_SVR / 4] |= 0x100u;
    ioapic_redirections = ((ioapic_read(IOAPIC_VER) >> 16) & 0xFFu) + 1u;
    apic_ready = 1;
    kprintf("APIC: enabled local APIC %u with %u IOAPIC entries\n",
            lapic_id, ioapic_redirections);
}

u8 apic_is_available(void) { return apic_ready; }
void apic_eoi(void) { if (lapic_base) lapic_base[APIC_REG_EOI / 4] = 0; }
u32 apic_cpu_id(void) {
    return lapic_base ? (lapic_base[APIC_REG_ID / 4] >> 24) : lapic_id;
}

u32 apic_cpu_count(void) { return cpu_count; }
u32 apic_online_cpu_count(void) { return online_cpu_count; }

static void apic_wait_delivery(void) {
    for (u32 delay = 0u; delay < 100000u; delay++) {
        if (!(lapic_base[APIC_REG_ICR_LOW / 4] & APIC_ICR_DELIVERY)) return;
        __asm__ volatile("pause");
    }
}

static void apic_send_ipi(u8 destination, u32 command) {
    lapic_base[APIC_REG_ICR_HIGH / 4] = (u32)destination << 24;
    lapic_base[APIC_REG_ICR_LOW / 4] = command;
    apic_wait_delivery();
}

static void ap_prepare_trampoline(u8 apic_id, uintptr_t stack_top) {
    u8 *params = (u8 *)(uintptr_t)AP_PARAMS_PHYS;
    u64 code32 = 0x00CF9A000000FFFFull;
    u64 data = 0x00CF92000000FFFFull;
    u64 code64 = 0x00AF9A000000FFFFull;
    u32 protected_offset = AP_TRAMPOLINE_PHYS +
        (u32)((uintptr_t)ap_protected_mode - (uintptr_t)ap_trampoline_start);
    u32 long_offset = AP_TRAMPOLINE_PHYS +
        (u32)((uintptr_t)ap_long_mode - (uintptr_t)ap_trampoline_start);
    uintptr_t cr3;

    (void)apic_id;
    memcpy((void *)(uintptr_t)AP_TRAMPOLINE_PHYS,
           &ap_trampoline_start,
           (size_t)(&ap_trampoline_end - &ap_trampoline_start));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    *(u32 *)(params + 0u) = (u32)cr3;
    *(u64 *)(params + 8u) = (u64)stack_top;
    *(u64 *)(params + 16u) = (u64)(uintptr_t)&ap_entry;
    *(u16 *)(params + 24u) = 31u;
    *(u32 *)(params + 26u) = AP_PARAMS_PHYS + 48u;
    memcpy(params + 48u, &code32, sizeof(code32));
    memcpy(params + 56u, &data, sizeof(data));
    memcpy(params + 64u, &code64, sizeof(code64));
    *(u16 *)(params + 30u) = 0x08u;
    *(u32 *)(params + 32u) = protected_offset;
    *(u16 *)(params + 34u) = 0x18u;
    *(u32 *)(params + 36u) = long_offset;
    *(u16 *)(params + 40u) = 0x18u;
}

void ap_entry(void) {
    disable_interrupts();
    gdt_load_current_cpu();
    idt_load_current_cpu();
    __sync_fetch_and_add(&online_cpu_count, 1u);
    for (;;) halt();
}

void apic_start_aps(void) {
    u32 started = 0u;
    if (!apic_ready || cpu_count < 2u) {
        kprintf("APIC: no secondary CPUs to start\n");
        return;
    }
    for (u32 i = 0u; i < cpu_count; i++) {
        if (cpu_ids[i] == lapic_id) continue;
        ap_prepare_trampoline(cpu_ids[i],
                              (uintptr_t)&ap_stacks[i][AP_STACK_SIZE]);
        apic_send_ipi(cpu_ids[i], APIC_ICR_INIT);
        for (volatile u32 delay = 0u; delay < 10000u; delay++) __asm__ volatile("pause");
        apic_send_ipi(cpu_ids[i], APIC_ICR_DEASSERT);
        apic_send_ipi(cpu_ids[i], APIC_ICR_STARTUP | (AP_TRAMPOLINE_PHYS >> 12));
        for (volatile u32 delay = 0u; delay < 10000u; delay++) __asm__ volatile("pause");
        apic_send_ipi(cpu_ids[i], APIC_ICR_STARTUP | (AP_TRAMPOLINE_PHYS >> 12));
        for (u32 delay = 0u; delay < 100000u; delay++) {
            if (online_cpu_count > started + 1u) break;
            __asm__ volatile("pause");
        }
        if (online_cpu_count > started + 1u) started++;
    }
    kprintf("APIC: %u/%u CPUs online; AP scheduler handoff pending\n",
            online_cpu_count, cpu_count);
}

void apic_register_irq(u8 irq, interrupt_handler_t handler) {
    u32 vector = irq < 16u ? 32u + irq : 48u + ((u32)irq - 16u);
    if (!handler || vector >= 64u) return;
    interrupt_install_handler(vector, handler);
    ioapic_route(irq, (u8)vector);
}