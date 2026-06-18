#include "pkg.h"
#include "kprintf.h"
#include "string.h"
#include "vfs.h"

typedef struct {
    const char *name;
    const char *version;
    const char *description;
    const char *file_path;
    const char *content;
} package_t;

static const package_t available_packages[] = {
    {"neofetch", "1.0.0", "Fast system information tool", "./usr/bin/neofetch",
     "#!/bin/gsh\necho \"   /\\_/\\   Galio OS v1.0.0\"\necho \"  ( o.o )  Kernel: Galio-x86\"\necho \"   > ^ <   Uptime: simulated\"\necho \"  Memory: 1024 MB\"\n"},
    {"calc", "1.1.0", "Terminal-based basic calculator", "./usr/bin/calc",
     "#!/bin/gsh\necho \"Galio Calculator v1.1.0\"\necho \"Usage: calc <expression>\"\n"},
    {"nano", "2.9.3", "Tiny and friendly text editor", "./usr/bin/nano",
     "#!/bin/gsh\necho \"Galio Nano Editor v2.9.3\"\n"},
    {"snake", "1.0.2", "Classic terminal snake game", "./usr/bin/snake",
     "#!/bin/gsh\necho \"Galio Snake v1.0.2\"\n"},
    {"netutils", "1.4.0", "Extra network diagnostic tools", "./usr/bin/netutils",
     "#!/bin/gsh\necho \"Galio Network Utilities v1.4.0\"\n"}
};

#define PKG_COUNT (sizeof(available_packages) / sizeof(available_packages[0]))

static void ensure_pkg_dirs(void) {
    vfs_mkdir("./var", 1);
    vfs_mkdir("./var/lib", 1);
    vfs_mkdir("./var/lib/pkg", 1);
    vfs_mkdir("./usr", 1);
    vfs_mkdir("./usr/bin", 1);
}

static u8 is_package_installed(const char *name) {
    ensure_pkg_dirs();
    char marker[128];
    strcpy(marker, "./var/lib/pkg/installed_");
    strncat(marker, name, sizeof(marker) - strlen(marker) - 1);
    vfs_entry_t *entry = vfs_find(marker);
    return (entry != NULL);
}

static void mark_package_installed(const char *name) {
    ensure_pkg_dirs();
    char marker[128];
    strcpy(marker, "./var/lib/pkg/installed_");
    strncat(marker, name, sizeof(marker) - strlen(marker) - 1);
    u32 fd = vfs_create(marker, 1);
    if (fd != 0xFFFFFFFFu) {
        vfs_write(fd, "1", 1);
        vfs_close(fd);
    }
}

static void unmark_package_installed(const char *name) {
    char marker[128];
    strcpy(marker, "./var/lib/pkg/installed_");
    strncat(marker, name, sizeof(marker) - strlen(marker) - 1);
    vfs_unlink(marker);
}

static const char *skip_spaces(const char *s) {
    while (s && *s == ' ') s++;
    return s;
}

static const char *copy_token(const char *src, char *dst, u32 max) {
    u32 i = 0;
    while (*src && *src != ' ' && i + 1 < max) {
        dst[i++] = *src++;
    }
    dst[i] = '\0';
    return src;
}

u8 shell_pkg_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (!args || *args == '\0') {
        kprintf("Usage: pkg <list|install|remove> [package_name]\n");
        return 0;
    }

    char subcommand[32];
    const char *next = copy_token(args, subcommand, sizeof(subcommand));
    next = skip_spaces(next);

    if (strcmp(subcommand, "list") == 0) {
        kprintf("Available packages:\n");
        kprintf("  %-12s %-8s %-10s %s\n", "NAME", "VERSION", "STATUS", "DESCRIPTION");
        kprintf("  ---------------------------------------------------------------\n");
        for (u32 i = 0; i < PKG_COUNT; i++) {
            u8 inst = is_package_installed(available_packages[i].name);
            kprintf("  %-12s %-8s %-10s %s\n",
                    available_packages[i].name,
                    available_packages[i].version,
                    inst ? "Installed" : "Available",
                    available_packages[i].description);
        }
        return 1;
    }

    if (strcmp(subcommand, "install") == 0) {
        char pkg_name[64];
        copy_token(next, pkg_name, sizeof(pkg_name));
        if (pkg_name[0] == '\0') {
            kprintf("Error: Please specify package name to install\n");
            return 0;
        }

        const package_t *target = NULL;
        for (u32 i = 0; i < PKG_COUNT; i++) {
            if (strcmp(available_packages[i].name, pkg_name) == 0) {
                target = &available_packages[i];
                break;
            }
        }

        if (!target) {
            kprintf("Error: Package '%s' not found in repository\n", pkg_name);
            return 0;
        }

        if (is_package_installed(pkg_name)) {
            kprintf("Package '%s' is already installed\n", pkg_name);
            return 1;
        }

        kprintf("Downloading '%s' package...\n", pkg_name);
        ensure_pkg_dirs();
        u32 fd = vfs_create(target->file_path, 1);
        if (fd == 0xFFFFFFFFu) {
            kprintf("Error: Failed to create file %s\n", target->file_path);
            return 0;
        }

        u32 content_len = strlen(target->content);
        vfs_write(fd, target->content, content_len);
        vfs_close(fd);

        mark_package_installed(pkg_name);
        kprintf("Package '%s' successfully installed to %s\n", pkg_name, target->file_path);
        return 1;
    }

    if (strcmp(subcommand, "remove") == 0) {
        char pkg_name[64];
        copy_token(next, pkg_name, sizeof(pkg_name));
        if (pkg_name[0] == '\0') {
            kprintf("Error: Please specify package name to remove\n");
            return 0;
        }

        const package_t *target = NULL;
        for (u32 i = 0; i < PKG_COUNT; i++) {
            if (strcmp(available_packages[i].name, pkg_name) == 0) {
                target = &available_packages[i];
                break;
            }
        }

        if (!target) {
            kprintf("Error: Package '%s' not found\n", pkg_name);
            return 0;
        }

        if (!is_package_installed(pkg_name)) {
            kprintf("Package '%s' is not installed\n", pkg_name);
            return 0;
        }

        kprintf("Removing '%s' package...\n", pkg_name);
        vfs_unlink(target->file_path);
        unmark_package_installed(pkg_name);
        kprintf("Package '%s' successfully removed\n", pkg_name);
        return 1;
    }

    kprintf("Unknown pkg subcommand. Usage: pkg <list|install|remove> [package_name]\n");
    return 0;
}
