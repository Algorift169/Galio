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

/* mkiofs.c - Generate InitRD filesystem image */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define VFS_MAGIC 0xDEADBEEF
#define VFS_VERSION 1
#define VFS_MAX_FILES 512
#define VFS_MAX_PATH 512
#define VFS_MAX_FILENAME 256

static void build_boot_time_string(char *out, size_t size) {
    const char *date = __DATE__; /* "Mmm dd yyyy" */
    const char *time = __TIME__; /* "hh:mm:ss" */
    char month_str[4] = {0};
    int day = 0;
    int year = 0;

    memcpy(month_str, date, 3);
    if (date[4] == ' ') {
        day = date[5] - '0';
    } else {
        day = (date[4] - '0') * 10 + (date[5] - '0');
    }
    year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0');

    int month = 1;
    const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    for (int i = 0; i < 12; i++) {
        if (strncmp(month_str, months[i], 3) == 0) {
            month = i + 1;
            break;
        }
    }

    snprintf(out, size, "%04d-%02d-%02d %s", year, month, day, time);
}

static char boot_config_txt[256] = {0};

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
    /* Root and core directories */
    {".",              NULL, 0, 1},
    {"./boot",          NULL, 0, 1},
    {"./gui",           NULL, 0, 1},
    {"./gui/boot",      NULL, 0, 1},
    {"./dev",           NULL, 0, 1},
    {"./etc",           NULL, 0, 1},
    {"./lib",           NULL, 0, 1},
    {"./mnt",           NULL, 0, 1},
    {"./media",         NULL, 0, 1},
    {"./root",          NULL, 0, 1},
    {"./run",           NULL, 0, 1},
    {"./srv",           NULL, 0, 1},
    {"./tmp",           NULL, 0, 1},
    {"./lost+found",    NULL, 0, 1},

    /* Built-in GUI assets */
    {"./assets",                NULL, 0, 1},
    {"./assets/wallpapers",     NULL, 0, 1},
    {"./assets/wallpapers/wal1.png", NULL, 0, 0},

    /* /usr subdirectories */
    {"./usr",           NULL, 0, 1},
    {"./usr/home",     NULL, 0, 1},
    {"./usr/lib",       NULL, 0, 1},
    {"./usr/share",     NULL, 0, 1},

    /* /var subdirectories */
    {"./var",           NULL, 0, 1},
    {"./var/log",       NULL, 0, 1},
    {"./var/account",   NULL, 0, 1},

    /* /etc subdirectories */
    {"./etc/network",   NULL, 0, 1},

    /* /home subdirectories (moved under /usr) */
    {"./usr/home/desktop",  NULL, 0, 1},
    {"./usr/home/desktop/recycle", NULL, 0, 1},
    {"./usr/home/downloads", NULL, 0, 1},
    {"./usr/home/music",    NULL, 0, 1},
    {"./usr/home/documents", NULL, 0, 1},
    {"./usr/home/videos",   NULL, 0, 1},
    {"./usr/home/recent",   NULL, 0, 1},
    {"./usr/home/images",   NULL, 0, 1},
    {"./usr/home/pictures", NULL, 0, 1},

    /* Boot & Config */
    {"./boot/.config.txt",
        boot_config_txt,
        0, 0},

    /* System files */
    {"./etc/.hostname", "galio\n", 14, 0},

    {"./etc/.galio-release",
        "NAME=\"Galio\"\n"
        "VERSION=\"0.1.0\"\n"
        "ID=\"galio\"\n"
        "PRETTY_NAME=\"Galio Operating System 0.1.0\"\n"
        "ARCHITECTURE=\"x86_64\"\n"
        "FILESYSTEM=\"vfs+ext2\"\n",
        111, 0},

    {"./etc/.issue",
        "Welcome to Galio Kernel v0.1.0\n"
        "Built for x86_64 architecture\n"
        "=================================\n",
        95, 0},

    {"./etc/welcome.txt",
        "╔═══════════════════════════════════════════╗\n"
        "║     Welcome to Galio Kernel v0.1.0        ║\n"
        "║     A Lightweight x86_64 OS Kernel        ║\n"
        "║                                           ║\n"
        "║     Filesystem: Fully Operational         ║\n"
        "║     Memory: Paging and physical frames    ║\n"
        "║     Input: PS/2 and device files           ║\n"
        "╚═══════════════════════════════════════════╝\n",
        282, 0},

    /* Boot banner */
    {"./boot/banner.txt",
        "=====================================\n"
        "  Galio Kernel\n"
        "  Version: 0.1.0 (Alpha)\n"
        "  Architecture: x86_64\n"
        "  Bootloader: GRUB Multiboot2\n"
        "=====================================\n",
        139, 0},

    /* User home files */
    {"./usr/home/Desktop/readme.txt",
        "Desktop Directory\n"
        "=================\n"
        "\n"
        "This is your desktop directory.\n"
        "Place your shortcuts and files here.\n",
        80, 0},

    /* Optional developer file: include host-collected wifi scan results
     * at build time by placing a file at tools/shell/wifi_scan.txt
     * The file will be embedded into the initrd as /etc/wifi_scan
     * and parsed by the kernel for development/testing. */
    {"./etc/wifi_scan", NULL, 0, 0},
};

static int file_count = sizeof(files) / sizeof(files[0]);

int main(int argc, char *argv[]) {
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s <output.bin> [asset-source]\n", argv[0]);
        return 1;
    }

    const char *output_file = argv[1];
    const char *asset_source = argc == 3 ? argv[2] : "assets/wallpapers/wal1.png";
    char *asset_data = NULL;
    FILE *asset_fp = fopen(asset_source, "rb");
    if (!asset_fp) {
        fprintf(stderr, "Cannot open asset source: %s\n", asset_source);
        return 1;
    }
    if (fseek(asset_fp, 0, SEEK_END) != 0) {
        fclose(asset_fp);
        fprintf(stderr, "Cannot seek asset source: %s\n", asset_source);
        return 1;
    }
    long asset_size = ftell(asset_fp);
    if (asset_size <= 0 || (unsigned long)asset_size > UINT_MAX) {
        fclose(asset_fp);
        fprintf(stderr, "Invalid asset size: %s\n", asset_source);
        return 1;
    }
    rewind(asset_fp);
    asset_data = malloc((size_t)asset_size);
    if (!asset_data || fread(asset_data, 1, (size_t)asset_size, asset_fp) != (size_t)asset_size) {
        free(asset_data);
        fclose(asset_fp);
        fprintf(stderr, "Cannot read asset source: %s\n", asset_source);
        return 1;
    }
    fclose(asset_fp);
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].path, "./assets/wallpapers/wal1.png") == 0) {
            files[i].data = asset_data;
            files[i].size = (unsigned int)asset_size;
            break;
        }
    }
    FILE *fp = fopen(output_file, "wb");
    if (!fp) {
        perror("Cannot open output file");
        free(asset_data);
        return 1;
    }

    /* Build a current initrd boot config with a runtime boot_time value. */
    {
        char boot_time_string[32];
        build_boot_time_string(boot_time_string, sizeof(boot_time_string));
        snprintf(boot_config_txt, sizeof(boot_config_txt),
                 "kernel=galio\n"
                 "version=0.1.0\n"
                 "arch=x86_64\n"
                 "bootloader=GRUB Multiboot2\n"
                 "boot_time=%s\n"
                 "timezone_offset_hours=6\n",
                 boot_time_string);
        for (int i = 0; i < file_count; i++) {
            if (strcmp(files[i].path, "./boot/.config.txt") == 0) {
                files[i].size = (unsigned int)strlen(boot_config_txt);
                break;
            }
        }
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
                        /* find the index for ./etc/wifi_scan */
                        for (int i = 0; i < file_count; i++) {
                            if (strcmp(files[i].path, "./etc/wifi_scan") == 0) {
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
        free(asset_data);
        return 1;
    }

    for (int i = 0; i < file_count; i++) {
        if (!files[i].is_dir && files[i].data && files[i].size > 0) {
            if (fwrite(files[i].data, files[i].size, 1, fp) != 1) {
                perror("Failed to write file data");
                fclose(fp);
                free(asset_data);
                return 1;
            }
        }
    }

    fclose(fp);
    free(asset_data);

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