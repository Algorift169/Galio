/* mkiofs.c - Generate InitRD filesystem image */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VFS_MAGIC 0xDEADBEEF
#define VFS_VERSION 1
#define VFS_MAX_FILES 512
#define VFS_MAX_PATH 512
#define VFS_MAX_FILENAME 256

typedef struct {
    char path[VFS_MAX_PATH];
    unsigned int size;
    unsigned int offset;
    unsigned int is_dir;
    unsigned int permissions;
} vfs_entry_t;

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int entry_count;
    unsigned int data_offset;
    vfs_entry_t entries[VFS_MAX_FILES];
} vfs_header_t;

typedef struct {
    const char *path;
    const char *data;
    unsigned int size;
    unsigned int is_dir;
} file_spec_t;

static file_spec_t files[] = {
    /* Directories */
    {"/",              NULL, 0, 1},
    {"/boot",          NULL, 0, 1},
    {"/bin",           NULL, 0, 1},
    {"/etc",           NULL, 0, 1},
    {"/lib",           NULL, 0, 1},
    {"/dev",           NULL, 0, 1},
    {"/tmp",           NULL, 0, 1},
    {"/home",          NULL, 0, 1},
    {"/home/Desktop",  NULL, 0, 1},
    {"/home/desktop",  NULL, 0, 1},
    {"/home/Desktop/recycle", NULL, 0, 1},
    {"/home/desktop/recycle", NULL, 0, 1},
    {"/home/downloads", NULL, 0, 1},
    {"/home/music",    NULL, 0, 1},
    {"/home/doccuments", NULL, 0, 1},
    {"/home/videos",   NULL, 0, 1},
    {"/home/recent",   NULL, 0, 1},
    {"/home/Downloads", NULL, 0, 1},
    {"/home/Images",   NULL, 0, 1},
    {"/home/Videos",   NULL, 0, 1},
    {"/home/Music",    NULL, 0, 1},
    {"/home/Documents", NULL, 0, 1},
    {"/home/Pictures", NULL, 0, 1},
    {"/usr",           NULL, 0, 1},
    {"/usr/bin",       NULL, 0, 1},
    {"/usr/lib",       NULL, 0, 1},
    {"/etc/init.d",    NULL, 0, 1},

    /* Boot & Config */
    {"/boot/config.txt",
        "kernel=galio\n"
        "version=0.1.0\n"
        "arch=x86-32\n"
        "bootloader=GRUB\n"
        "boot_time=2026-05-09\n",
        91, 0},

    {"/boot/grub.cfg",
        "menuentry 'Galio Kernel' {\n"
        "    multiboot /boot/galio.bin\n"
        "}\n",
        49, 0},

    /* System files */
    {"/etc/hostname", "galio\n", 14, 0},

    {"/etc/os-release",
        "NAME=\"Galio \"\n"
        "VERSION=\"0.1.0\"\n"
        "ID=\"galio\"\n"
        "PRETTY_NAME=\"Galio 0.1.0\"\n",
        79, 0},

    {"/etc/issue",
        "Welcome to Galio Kernel v0.1.0\n"
        "Built on x86 32-bit architecture\n"
        "=================================\n",
        95, 0},

    {"/etc/welcome.txt",
        "╔═══════════════════════════════════════════╗\n"
        "║     Welcome to Galio Kernel v0.1.0        ║\n"
        "║     A Lightweight 32-bit OS Kernel        ║\n"
        "║                                           ║\n"
        "║     Filesystem: Fully Operational         ║\n"
        "║     Memory: Protected Mode                ║\n"
        "║     IRQ: 100 Hz Timer                     ║\n"
        "╚═══════════════════════════════════════════╝\n",
        282, 0},

    /* Boot banner */
    {"/boot/banner.txt",
        "=====================================\n"
        "  Galio  Kernel\n"
        "  Version: 0.1.0 (Alpha)\n"
        "  Architecture: x86 32-bit\n"
        "  Bootloader: GRUB Multiboot\n"
        "=====================================\n",
        139, 0},

    /* Process info */
    {"/proc/cpuinfo",
        "processor   : 0\n"
        "vendor_id   : GenuineIntel\n"
        "cpu family  : 6\n"
        "stepping    : 0\n",
        83, 0},

    /* Log files */
    {"/var/log/boot.log",
        "[BOOT] Kernel initialized\n"
        "[BOOT] VFS mounted successfully\n"
        "[BOOT] Memory manager ready\n"
        "[BOOT] Filesystem test passed\n",
        111, 0},

    {"/var/log/system.log",
        "[SYSTEM] Uptime: 0 seconds\n"
        "[SYSTEM] Memory usage: 2.5 MB\n"
        "[SYSTEM] Processes: 1\n"
        "[SYSTEM] VFS entries: 50+\n",
        98, 0},

    /* User home files */
    {"/home/Desktop/readme.txt",
        "Desktop Directory\n"
        "=================\n"
        "\n"
        "This is your desktop directory.\n"
        "Place your shortcuts and files here.\n",
        80, 0},

    /* System binaries (placeholder) */
    {"/bin/init",
        "#!/bin/galio\n"
        "# Init script\n"
        "mount_all\n"
        "start_services\n",
        49, 0},

    {"/bin/sh",
        "#!/bin/galio\n"
        "# Shell executable\n"
        "# Interactive shell for Galio\n",
        49, 0},

    /* Libraries info */
    {"/lib/version",
        "libc version 1.0\n"
        "Standard library for Galio\n",
        49, 0},

    {"/usr/lib/modules.txt",
        "Loaded modules:\n"
        "  vfs.ko - Virtual Filesystem\n"
        "  mem.ko - Memory Manager\n"
        "  proc.ko - Process Manager\n",
        89, 0},

    /* Temporary directory (can be empty) */
    {"/tmp/test.tmp",
        "Temporary file for testing\n"
        "This can be deleted anytime\n",
        57, 0},

    /* System info */
    {"/sys/kernel/version",
        "Galio Kernel v0.1.0\n",
        21, 0},

    {"/sys/memory/total",
        "128 MB\n",
        8, 0},

    {"/sys/memory/free",
        "120 MB\n",
        8, 0},

    /* Additional documentation */
    {"/etc/init.d/network",
        "#!/bin/sh\n"
        "# Network initialization script\n"
        "echo 'Network services starting...'\n",
        65, 0},

    {"/etc/init.d/filesystem",
        "#!/bin/sh\n"
        "# Filesystem initialization script\n"
        "echo 'Mounting filesystems...'\n",
        67, 0},
    /* Optional developer file: include host-collected wifi scan results
     * at build time by placing a file at tools/shell/wifi_scan.txt
     * The file will be embedded into the initrd as /etc/wifi_scan
     * and parsed by the kernel for development/testing. */
    {"/etc/wifi_scan", NULL, 0, 0},
};

static int file_count = sizeof(files) / sizeof(files[0]);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output.bin>\n", argv[0]);
        return 1;
    }

    const char *output_file = argv[1];
    FILE *fp = fopen(output_file, "wb");
    if (!fp) {
        perror("Cannot open output file");
        return 1;
    }

    /* If a developer wifi scan file exists, read and attach it to the
     * corresponding entry so it becomes part of the initrd image. */
    {
        const char *local = "tools/shell/wifi_scan.txt";
        FILE *wf = fopen(local, "rb");
        if (wf) {
            fseek(wf, 0, SEEK_END);
            long sz = ftell(wf);
            fseek(wf, 0, SEEK_SET);
            if (sz > 0) {
                char *buf = malloc(sz);
                if (buf) {
                    if (fread(buf, 1, sz, wf) == (size_t)sz) {
                        /* find the index for /etc/wifi_scan */
                        for (int i = 0; i < file_count; i++) {
                            if (strcmp(files[i].path, "/etc/wifi_scan") == 0) {
                                files[i].data = buf;
                                files[i].size = (unsigned int)sz;
                                break;
                            }
                        }
                    } else {
                        free(buf);
                    }
                }
            }
            fclose(wf);
        }
    }

    vfs_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = VFS_MAGIC;
    header.version = VFS_VERSION;
    header.entry_count = file_count;

    unsigned int data_offset = sizeof(vfs_header_t);
    unsigned int current_offset = data_offset;

    for (int i = 0; i < file_count; i++) {
        strncpy(header.entries[i].path, files[i].path, VFS_MAX_PATH - 1);
        header.entries[i].is_dir = files[i].is_dir;
        header.entries[i].permissions = files[i].is_dir ? 0755 : 0644;

        if (files[i].is_dir) {
            header.entries[i].size = 0;
            header.entries[i].offset = 0;
        } else {
            header.entries[i].size = files[i].size;
            header.entries[i].offset = current_offset;
            current_offset += files[i].size;
        }
    }

    header.data_offset = data_offset;

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        perror("Failed to write header");
        fclose(fp);
        return 1;
    }

    for (int i = 0; i < file_count; i++) {
        if (!files[i].is_dir && files[i].data && files[i].size > 0) {
            if (fwrite(files[i].data, files[i].size, 1, fp) != 1) {
                perror("Failed to write file data");
                fclose(fp);
                return 1;
            }
        }
    }

    fclose(fp);

    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           Galio Filesystem Image Generator (mkiofs)            ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Generated: %s\n", output_file);
    printf("║ Header size: %zu bytes\n", sizeof(vfs_header_t));
    printf("║ Total entries: %d\n", file_count);
    printf("║ Data offset: 0x%X\n", data_offset);
    printf("║ Image size: %u bytes\n", current_offset);
    printf("╠════════════════════════════════════════════════════════════════╣\n");

    int dirs = 0, data_files = 0;
    unsigned int total_data = 0;
    for (int i = 0; i < file_count; i++) {
        if (files[i].is_dir) dirs++;
        else {
            data_files++;
            total_data += files[i].size;
        }
    }

    printf("║ Filesystem Contents:\n");
    printf("║   Directories: %d\n", dirs);
    printf("║   Data files: %d\n", data_files);
    printf("║   Total data: %u bytes\n", total_data);
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Directory Tree:\n");
    for (int i = 0; i < file_count; i++) {
        if (files[i].is_dir) {
            printf("║   [DIR]  %s/\n", files[i].path);
        }
    }
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ File List:\n");
    for (int i = 0; i < file_count; i++) {
        if (!files[i].is_dir) {
            printf("║   [FILE] %-35s %8u bytes\n",
                   files[i].path, files[i].size);
        }
    }
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    return 0;
}