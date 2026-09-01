#include "cpufreq_cmd.h"
#include "cpufreq/cpufreq.h"
#include "cpufreq/policy.h"
#include "cpufreq/governor.h"
#include "cpufreq/stats.h"
#include "cpu/capabilities.h"
#include "auth.h"
#include "kprintf.h"
#include "string.h"

static const char *skip_spaces(const char *text) {
    while (text && (*text == ' ' || *text == '\t')) text++;
    return text;
}

static const char *next_word(const char *text, char *word, u32 word_size) {
    u32 length = 0;
    text = skip_spaces(text);
    while (text && text[length] && text[length] != ' ' && text[length] != '\t') length++;
    if (length == 0 || length >= word_size) return NULL;
    strncpy(word, text, length);
    word[length] = 0;
    return text + length;
}

static u8 parse_mhz(const char *text, u64 *mhz) {
    u64 value = 0;
    if (!text || !*text || !mhz) return 0;
    while (*text) {
        if (*text < '0' || *text > '9') return 0;
        if (value > (0xFFFFFFFFFFFFFFFFULL - (u64)(*text - '0')) / 10) return 0;
        value = value * 10 + (u64)(*text - '0');
        text++;
    }
    if (value == 0 || value > 0xFFFFFFFFFFFFULL) return 0;
    *mhz = value;
    return 1;
}

static void print_summary(void) {
    cpufreq_policy_t *policy = cpufreq_policy_get(0);
    u64 current = 0;
    const cpufreq_driver_t *driver = cpufreq_active_driver();
    if (!policy || !driver) {
        kprintf("CPU 0: driver=none current=unavailable min=unavailable max=unavailable governor=%s\n",
            cpufreq_governor_name(cpufreq_active_governor()));
        return;
    }
    cpufreq_get_current(0, &current);
    kprintf("CPU 0: driver=%s current=%llu kHz min=%llu kHz max=%llu kHz governor=%s\n",
            driver->name, current, policy->min_khz, policy->max_khz,
            cpufreq_governor_name(policy->governor));
}

static void print_help(void) {
    kprintf("Usage: cpufreq <info|status|current|min|max|policy|governor|set|stats>\n");
    kprintf("  cpufreq governor [performance|powersave|userspace|ondemand]\n");
    kprintf("  cpufreq set <MHz>\n");
}

void shell_cpufreq_command(const char *args) {
    char command[32];
    args = next_word(args, command, sizeof(command));
    if (!args) {
        print_summary();
        return;
    }
    args = skip_spaces(args);

    if (strcmp(command, "info") == 0 || strcmp(command, "status") == 0 || strcmp(command, "policy") == 0) {
        const cpu_capabilities_t *capabilities = cpu_get_capabilities();
        print_summary();
        kprintf("vendor=%s family=%u model=%u stepping=%u MSR=%s APERF/MPERF=%s HWP=%s\n",
                capabilities->vendor, capabilities->family, capabilities->model,
                capabilities->stepping, capabilities->has_msr ? "yes" : "no",
                capabilities->has_aperf_mperf ? "yes" : "no",
                capabilities->has_hwp ? "yes" : "no");
        kprintf("hardware frequency control: unsupported unless a verified platform driver is added\n");
    } else if (strcmp(command, "current") == 0) {
        u64 current = 0;
        cpufreq_status_t status = cpufreq_get_current(0, &current);
        if (status == CPUFREQ_OK) kprintf("Current: %llu kHz\n", current);
        else kprintf("Current: unavailable (%s)\n", cpufreq_status_string(status));
    } else if (strcmp(command, "min") == 0 || strcmp(command, "max") == 0) {
        cpufreq_policy_t *policy = cpufreq_policy_get(0);
        if (!policy || policy->min_khz == 0 || policy->max_khz == 0) {
            kprintf("%s: unavailable (hardware limits not reported)\n", command);
        }
        else kprintf("%s: %llu kHz\n", command, strcmp(command, "min") == 0 ? policy->min_khz : policy->max_khz);
    } else if (strcmp(command, "governor") == 0) {
        char governor_name[32];
        if (!*args) {
            kprintf("Governor: %s\n", cpufreq_governor_name(cpufreq_active_governor()));
        } else if (!auth_is_authorized()) {
            kprintf("cpufreq: changing governor requires rex\n");
        } else if (!next_word(args, governor_name, sizeof(governor_name)) ||
                   cpufreq_set_governor(governor_name) != CPUFREQ_OK) {
            kprintf("cpufreq: unknown governor '%s'\n", args);
        } else {
            kprintf("Governor: %s\n", governor_name);
        }
    } else if (strcmp(command, "set") == 0) {
        u64 mhz;
        if (!auth_is_authorized()) {
            kprintf("cpufreq: setting frequency requires rex\n");
        } else if (!parse_mhz(args, &mhz)) {
            kprintf("cpufreq: invalid frequency\nUsage: cpufreq set <MHz>\n");
        } else {
            cpufreq_status_t status = cpufreq_set_frequency(0, mhz * 1000);
            kprintf("cpufreq: set %llu MHz: %s\n", mhz, cpufreq_status_string(status));
        }
    } else if (strcmp(command, "stats") == 0) {
        kprintf("Transitions: %llu\nMin observed: %llu kHz\nMax observed: %llu kHz\n",
                cpufreq_stats_transition_count(), cpufreq_stats_min_observed(),
                cpufreq_stats_max_observed());
    } else if (strcmp(command, "help") == 0) {
        print_help();
    } else {
        kprintf("cpufreq: unknown command '%s'\n", command);
        print_help();
    }
}
