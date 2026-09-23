#include "acpi/acpi.h"
#include "kprintf.h"
#include "string.h"
#include "arch/x86/cpu.h"

#define ACPI_S3 3u
#define ACPI_S5 5u
#define ACPI_PM1_CNT_SLP_TYP_SHIFT 10u
#define ACPI_PM1_CNT_SLP_EN (1u << 13)

static u8 acpi_ready;
static u8 acpi_sleep_s3;
static u8 acpi_sleep_s5;
static u8 acpi_s3_type;
static u8 acpi_s5_type;
static u16 acpi_pm1a;
static u16 acpi_pm1b;
static u8 acpi_pm1_length;
static u64 acpi_mcfg;
static u64 acpi_hpet;

static u8 acpi_checksum(const u8 *data, u32 length) {
    u8 sum = 0u;
    for (u32 index = 0u; index < length; index++) sum = (u8)(sum + data[index]);
    return sum == 0u;
}

static u32 acpi_u32(const u8 *data) {
    return (u32)data[0] | ((u32)data[1] << 8) |
           ((u32)data[2] << 16) | ((u32)data[3] << 24);
}

static u64 acpi_u64(const u8 *data) {
    return (u64)acpi_u32(data) | ((u64)acpi_u32(data + 4) << 32);
}

static u8 acpi_has_signature(const u8 *data, const char *signature) {
    for (u32 index = 0u; index < 8u; index++) {
        if (data[index] != (u8)signature[index]) return 0u;
    }
    return 1u;
}

static u8 acpi_name_equal(const u8 *data, const char *name) {
    for (u32 index = 0u; index < 4u; index++) {
        if (data[index] != (u8)name[index]) return 0u;
    }
    return 1u;
}

static u8 *acpi_find_rsdp(void) {
    u16 ebda_segment = *(volatile u16 *)(uintptr_t)0x40Eu;
    u32 ebda = (u32)ebda_segment << 4;
    u8 *candidate;

    if (ebda >= 0x400u && ebda < 0xA0000u) {
        for (candidate = (u8 *)(uintptr_t)ebda;
             candidate < (u8 *)(uintptr_t)(ebda + 1024u); candidate += 16u) {
            if (acpi_has_signature(candidate, "RSD PTR ") &&
                acpi_checksum(candidate, 20u)) return candidate;
        }
    }
    for (candidate = (u8 *)(uintptr_t)0xE0000u;
         candidate < (u8 *)(uintptr_t)0x100000u; candidate += 16u) {
        if (acpi_has_signature(candidate, "RSD PTR ") &&
            acpi_checksum(candidate, 20u)) return candidate;
    }
    return NULL;
}

static u8 *acpi_find_table(u8 *root, const char *signature) {
    u32 root_length = acpi_u32(root + 4u);
    u32 entry_size = acpi_has_signature(root, "XSDT    ") ? 8u : 4u;
    u32 entries;
    if (root_length < 36u || !acpi_checksum(root, root_length)) return NULL;
    entries = (root_length - 36u) / entry_size;
    for (u32 index = 0u; index < entries; index++) {
        u64 address = entry_size == 8u ? acpi_u64(root + 36u + index * 8u) :
                      acpi_u32(root + 36u + index * 4u);
        u8 *table;
        u32 length;
        if (address > 0xFFFFFFFFull) continue;
        table = (u8 *)(uintptr_t)address;
        if (!acpi_has_signature(table, signature)) continue;
        length = acpi_u32(table + 4u);
        if (length >= 36u && acpi_checksum(table, length)) return table;
    }
    return NULL;
}

static u8 acpi_decode_integer(const u8 *data, u32 length, u32 *value) {
    if (!data || !value || length == 0u) return 0u;
    switch (data[0]) {
        case 0x00: *value = 0u; return 1u;
        case 0x01: *value = 1u; return 1u;
        case 0x0A: if (length < 2u) return 0u; *value = data[1]; return 1u;
        case 0x0B: if (length < 3u) return 0u; *value = data[1] | ((u32)data[2] << 8); return 1u;
        case 0x0C: if (length < 5u) return 0u; *value = acpi_u32(data + 1u); return 1u;
        default: return 0u;
    }
}

static u8 acpi_find_sleep_object(const u8 *dsdt, u32 length, u8 state, u8 *type) {
    char name[5] = {'_', (char)('0' + state), '_', '_', 0};
    for (u32 index = 0u; index + 8u < length; index++) {
        u32 package_index;
        u32 package_length;
        u32 value;
        if (dsdt[index] != 0x08u || !acpi_name_equal(dsdt + index + 1u, name)) continue;
        package_index = index + 5u;
        if (dsdt[package_index] != 0x12u) continue;
        package_length = dsdt[package_index + 1u] & 0x3Fu;
        if (package_length == 0u || package_index + 2u + package_length > length) continue;
        if (!acpi_decode_integer(dsdt + package_index + 2u,
                                 length - package_index - 2u, &value)) continue;
        *type = (u8)(value & 7u);
        return 1u;
    }
    return 0u;
}

int acpi_init(void) {
    u8 *rsdp = acpi_find_rsdp();
    u8 *root;
    u8 *fadt;
    u8 *dsdt;
    u64 root_address;
    u32 fadt_length;
    u32 dsdt_address;

    acpi_ready = 0u;
    acpi_sleep_s3 = 0u;
    acpi_sleep_s5 = 0u;
    acpi_mcfg = 0u;
    acpi_hpet = 0u;
    if (!rsdp) {
        kprintf("ACPI: RSDP not found; legacy power controls only\n");
        return -1;
    }
    if (rsdp[15] >= 2u && acpi_u32(rsdp + 20u) >= 36u && acpi_checksum(rsdp, acpi_u32(rsdp + 20u)))
        root_address = acpi_u64(rsdp + 24u);
    else if (acpi_checksum(rsdp, 20u))
        root_address = acpi_u32(rsdp + 16u);
    else {
        kprintf("ACPI: invalid RSDP; legacy power controls only\n");
        return -1;
    }
    if (root_address > 0xFFFFFFFFull) {
        kprintf("ACPI: root table is above the supported physical address range\n");
        return -1;
    }
    root = (u8 *)(uintptr_t)root_address;
    if (!acpi_has_signature(root, "RSDT    ") && !acpi_has_signature(root, "XSDT    ")) {
        kprintf("ACPI: invalid root table; legacy power controls only\n");
        return -1;
    }
    fadt = acpi_find_table(root, "FACP    ");
    if (!fadt) {
        kprintf("ACPI: FADT not found; suspend unavailable\n");
        return -1;
    }
    fadt_length = acpi_u32(fadt + 4u);
    if (fadt_length < 92u) {
        kprintf("ACPI: FADT is too short; suspend unavailable\n");
        return -1;
    }
    acpi_pm1a = (u16)acpi_u32(fadt + 64u);
    acpi_pm1b = (u16)acpi_u32(fadt + 68u);
    acpi_pm1_length = fadt[89u];
    dsdt_address = acpi_u32(fadt + 40u);
    dsdt = (u8 *)(uintptr_t)dsdt_address;
    if (dsdt_address && acpi_u32(dsdt + 4u) >= 36u &&
        acpi_checksum(dsdt, acpi_u32(dsdt + 4u))) {
        u32 dsdt_length = acpi_u32(dsdt + 4u);
        acpi_sleep_s3 = acpi_find_sleep_object(dsdt, dsdt_length, ACPI_S3, &acpi_s3_type);
        acpi_sleep_s5 = acpi_find_sleep_object(dsdt, dsdt_length, ACPI_S5, &acpi_s5_type);
    }
    {
        u8 *mcfg = acpi_find_table(root, "MCFG    ");
        if (mcfg && acpi_u32(mcfg + 4u) >= 44u && acpi_u64(mcfg + 44u) <= 0xFFFFFFFFull)
            acpi_mcfg = acpi_u64(mcfg + 44u);
    }
    {
        u8 *hpet = acpi_find_table(root, "HPET    ");
        if (hpet && acpi_u32(hpet + 4u) >= 56u && hpet[40u] == 0u)
            acpi_hpet = acpi_u64(hpet + 44u);
    }
    acpi_ready = 1u;
    kprintf("ACPI: FADT PM1a=0x%x PM1b=0x%x width=%u S3=%s S5=%s\n",
            acpi_pm1a, acpi_pm1b, acpi_pm1_length,
            acpi_sleep_s3 ? "available" : "unavailable",
            acpi_sleep_s5 ? "available" : "unavailable");
    if (acpi_mcfg) kprintf("ACPI: MCFG ECAM base=0x%x\n", (u32)acpi_mcfg);
    if (acpi_hpet) kprintf("ACPI: HPET base=0x%x\n", (u32)acpi_hpet);
    return 0;
}

u8 acpi_is_available(void) { return acpi_ready; }
u8 acpi_has_sleep_state(u8 state) {
    if (state == ACPI_S3) return acpi_sleep_s3;
    if (state == ACPI_S5) return acpi_sleep_s5;
    return 0u;
}
u64 acpi_mcfg_base(void) { return acpi_mcfg; }
u64 acpi_hpet_base(void) { return acpi_hpet; }
u16 acpi_pm1a_control_port(void) { return acpi_pm1a; }
u16 acpi_pm1b_control_port(void) { return acpi_pm1b; }
u16 acpi_pm1_control_length(void) { return acpi_pm1_length; }

int acpi_enter_sleep(u8 state) {
    u8 sleep_type;
    u16 value;
    u16 current;
    if (state == ACPI_S3 && !acpi_sleep_s3) return -38;
    if (state == ACPI_S5 && !acpi_sleep_s5) return -38;
    sleep_type = state == ACPI_S3 ? acpi_s3_type : acpi_s5_type;
    if (!acpi_pm1a || acpi_pm1_length < 2u) return -38;
    current = inw(acpi_pm1a);
    value = (u16)((current & ~(0x7u << ACPI_PM1_CNT_SLP_TYP_SHIFT)) |
                  ((u16)sleep_type << ACPI_PM1_CNT_SLP_TYP_SHIFT) |
                  ACPI_PM1_CNT_SLP_EN);
    outw(acpi_pm1a, value);
    if (acpi_pm1b) {
        current = inw(acpi_pm1b);
        value = (u16)((current & ~(0x7u << ACPI_PM1_CNT_SLP_TYP_SHIFT)) |
                      ((u16)sleep_type << ACPI_PM1_CNT_SLP_TYP_SHIFT) |
                      ACPI_PM1_CNT_SLP_EN);
        outw(acpi_pm1b, value);
    }
    return 0;
}
