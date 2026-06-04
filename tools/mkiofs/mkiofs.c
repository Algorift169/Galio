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
    {"/",              NULL, 0, 1},
    {"/boot",          NULL, 0, 1},
    {"/bin",           NULL, 0, 1},
    {"/sbin",          NULL, 0, 1},
    {"/dev",           NULL, 0, 1},
    {"/etc",           NULL, 0, 1},
    {"/home",          NULL, 0, 1},
    {"/lib",           NULL, 0, 1},
    {"/mnt",           NULL, 0, 1},
    {"/media",         NULL, 0, 1},
    {"/proc",          NULL, 0, 1},
    {"/root",          NULL, 0, 1},
    {"/run",           NULL, 0, 1},
    {"/srv",           NULL, 0, 1},
    {"/sys",           NULL, 0, 1},
    {"/tmp",           NULL, 0, 1},
    {"/fuse",          NULL, 0, 1},
    {"/lost+found",    NULL, 0, 1},

    /* /usr subdirectories */
    {"/usr",           NULL, 0, 1},
    {"/usr/bin",       NULL, 0, 1},
    {"/usr/sbin",      NULL, 0, 1},
    {"/usr/lib",       NULL, 0, 1},
    {"/usr/local",     NULL, 0, 1},
    {"/usr/local/bin", NULL, 0, 1},
    {"/usr/local/sbin", NULL, 0, 1},
    {"/usr/local/lib", NULL, 0, 1},
    {"/usr/local/share", NULL, 0, 1},
    {"/usr/share",     NULL, 0, 1},
    {"/usr/share/doc", NULL, 0, 1},
    {"/usr/share/man", NULL, 0, 1},
    {"/usr/share/info", NULL, 0, 1},
    {"/usr/include",   NULL, 0, 1},
    {"/usr/src",       NULL, 0, 1},
    {"/usr/games",     NULL, 0, 1},
    {"/usr/libexec",   NULL, 0, 1},

    /* /var subdirectories */
    {"/var",           NULL, 0, 1},
    {"/var/log",       NULL, 0, 1},
    {"/var/run",       NULL, 0, 1},
    {"/var/spool",     NULL, 0, 1},
    {"/var/spool/cron", NULL, 0, 1},
    {"/var/spool/mail", NULL, 0, 1},
    {"/var/crash",     NULL, 0, 1},
    {"/var/lock",      NULL, 0, 1},
    {"/var/account",   NULL, 0, 1},
    {"/var/mail",      NULL, 0, 1},
    {"/var/tmp",       NULL, 0, 1},
    {"/var/cache",     NULL, 0, 1},
    {"/var/cache/apt", NULL, 0, 1},
    {"/var/games",     NULL, 0, 1},

    /* /etc subdirectories */
    {"/etc/X11",       NULL, 0, 1},
    {"/etc/X11/xorg.conf.d", NULL, 0, 1},
    {"/etc/opt",       NULL, 0, 1},
    {"/etc/sgml",      NULL, 0, 1},
    {"/etc/init.d",    NULL, 0, 1},
    {"/etc/rc.d",      NULL, 0, 1},
    {"/etc/rc.d/init.d", NULL, 0, 1},
    {"/etc/share",     NULL, 0, 1},
    {"/etc/xml",       NULL, 0, 1},
    {"/etc/ssl",       NULL, 0, 1},
    {"/etc/ssl/certs", NULL, 0, 1},
    {"/etc/ssl/private", NULL, 0, 1},
    {"/etc/skel",      NULL, 0, 1},
    {"/etc/pam.d",     NULL, 0, 1},
    {"/etc/sysconfig", NULL, 0, 1},
    {"/etc/cron.d",    NULL, 0, 1},
    {"/etc/cron.daily", NULL, 0, 1},
    {"/etc/cron.weekly", NULL, 0, 1},
    {"/etc/cron.monthly", NULL, 0, 1},
    {"/etc/cron.hourly", NULL, 0, 1},
    {"/etc/security",  NULL, 0, 1},
    {"/etc/selinux",   NULL, 0, 1},
    {"/etc/iptables",  NULL, 0, 1},
    {"/etc/network",   NULL, 0, 1},
    {"/etc/network/if-up.d", NULL, 0, 1},
    {"/etc/network/if-down.d", NULL, 0, 1},
    {"/etc/network/if-pre-up.d", NULL, 0, 1},
    {"/etc/network/if-post-down.d", NULL, 0, 1},
    {"/etc/profile.d", NULL, 0, 1},
    {"/etc/modprobe.d", NULL, 0, 1},
    {"/etc/ssh",       NULL, 0, 1},

    /* /opt subdirectories */
    {"/opt",           NULL, 0, 1},
    {"/opt/bin",       NULL, 0, 1},
    {"/opt/sbin",      NULL, 0, 1},
    {"/opt/etc",       NULL, 0, 1},
    {"/opt/var",       NULL, 0, 1},
    {"/opt/lib",       NULL, 0, 1},
    {"/opt/share",     NULL, 0, 1},
    {"/opt/share/doc", NULL, 0, 1},
    {"/opt/share/man", NULL, 0, 1},
    {"/opt/local",     NULL, 0, 1},
    {"/opt/src",       NULL, 0, 1},

    /* /home subdirectories */
    {"/home/desktop",  NULL, 0, 1},
    {"/home/desktop/recycle", NULL, 0, 1},
    {"/home/downloads", NULL, 0, 1},
    {"/home/music",    NULL, 0, 1},
    {"/home/documents", NULL, 0, 1},
    {"/home/videos",   NULL, 0, 1},
    {"/home/recent",   NULL, 0, 1},
    {"/home/images",   NULL, 0, 1},
    {"/home/pictures", NULL, 0, 1},

    /* Boot & Config */
    {"/boot/config.txt",
        boot_config_txt,
        0, 0},

    {"/boot/grub.cfg",
        "menuentry 'Galio Kernel' {\n"
        "    multiboot /boot/galio.bin\n"
        "}\n",
        49, 0},

    /* System files */
    {"/etc/hostname", "galio\n", 14, 0},

    {"/etc/fstab",
        "# Galio Filesystem Table\n"
        "# <filesystem> <mount-point> <type> <options> <dump> <pass>\n"
        "/dev/sda1   /           ext2    defaults    0       1\n"
        "/dev/sda2   /var        ext2    defaults    0       2\n"
        "/dev/sda3   /home       ext2    defaults    0       2\n"
        "/proc       /proc       proc    defaults    0       0\n"
        "/sys        /sys        sysfs   defaults    0       0\n"
        "tmpfs       /tmp        tmpfs   defaults    0       0\n"
        "tmpfs       /run        tmpfs   defaults    0       0\n",
        250, 0},

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

    {"/etc/profile",
        "# /etc/profile: system-wide .profile file for the Bourne shell (sh(1))\n"
        "# and Bourne compatible shells (bash(1), ksh(1), pdksh(1), etc.)\n"
        "export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/opt/bin\n"
        "export LANG=en_US.UTF-8\n"
        "export TERM=linux\n",
        220, 0},

    {"/etc/bashrc",
        "# System-wide bashrc file\n"
        "# Executed for non-login shells\n"
        "alias ls='ls --color=auto'\n"
        "alias ll='ls -l'\n"
        "alias la='ls -la'\n"
        "alias rm='rm -i'\n"
        "alias mv='mv -i'\n"
        "alias cp='cp -i'\n",
        180, 0},

    {"/etc/sysctl.conf",
        "# /etc/sysctl.conf - kernel runtime parameters\n"
        "# kernel.sysrq = 1\n"
        "# net.ipv4.ip_forward = 0\n"
        "# net.ipv4.conf.default.rp_filter = 1\n"
        "# net.ipv4.tcp_syncookies = 1\n",
        180, 0},

    {"/etc/passwd",
        "root:x:0:0:root:/root:/bin/sh\n"
        "nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin\n",
        80, 0},

    {"/etc/group",
        "root:x:0:\n"
        "bin:x:1:\n"
        "daemon:x:2:\n"
        "sys:x:3:\n"
        "adm:x:4:\n"
        "tty:x:5:\n"
        "disk:x:6:\n"
        "lp:x:7:\n"
        "mail:x:8:\n"
        "news:x:9:\n"
        "uucp:x:10:\n"
        "man:x:12:\n"
        "proxy:x:13:\n"
        "kmem:x:15:\n"
        "dialout:x:20:\n"
        "fax:x:21:\n"
        "voice:x:22:\n"
        "cdrom:x:24:\n"
        "floppy:x:25:\n"
        "tape:x:26:\n"
        "sudo:x:27:\n"
        "audio:x:29:\n"
        "dip:x:30:\n"
        "www-data:x:33:\n"
        "backup:x:34:\n"
        "operator:x:37:\n"
        "list:x:38:\n"
        "irc:x:39:\n"
        "gnats:x:41:\n"
        "nobody:x:65534:\n",
        550, 0},

    /* Network configuration */
    {"/etc/network/interfaces",
        "# /etc/network/interfaces - Galio network configuration\n"
        "# This file describes the network interfaces available on this system\n"
        "auto lo\n"
        "iface lo inet loopback\n"
        "\n"
        "# Primary network interface\n"
        "#auto eth0\n"
        "#iface eth0 inet dhcp\n",
        250, 0},

    {"/etc/network/if-up.d/avahi-daemon",
        "#!/bin/sh\n"
        "# avahi-daemon startup\n"
        "# TODO: start avahi daemon\n",
        60, 0},

    {"/etc/ssh/sshd_config",
        "# Galio SSH Daemon Configuration\n"
        "Port 22\n"
        "Protocol 2\n"
        "HostKey /etc/ssh/ssh_host_rsa_key\n"
        "HostKey /etc/ssh/ssh_host_ecdsa_key\n"
        "# PermitRootLogin yes\n"
        "PubkeyAuthentication yes\n"
        "PasswordAuthentication yes\n",
        250, 0},

    {"/etc/security/limits.conf",
        "# /etc/security/limits.conf\n"
        "# <domain> <type> <item> <value>\n"
        "*         soft   nofile  65535\n"
        "*         hard   nofile  65535\n"
        "*         soft   nproc   4096\n"
        "*         hard   nproc   4096\n",
        180, 0},

    {"/etc/cron.d/hourly",
        "SHELL=/bin/sh\n"
        "PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin\n"
        "# Run scripts in /etc/cron.hourly\n"
        "0 * * * * root run-parts --report /etc/cron.hourly\n",
        150, 0},

    {"/etc/cron.d/daily",
        "SHELL=/bin/sh\n"
        "PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin\n"
        "# Run scripts in /etc/cron.daily\n"
        "0 2 * * * root run-parts --report /etc/cron.daily\n",
        150, 0},

    /* Boot banner */
    {"/boot/banner.txt",
        "=====================================\n"
        "  Galio Kernel\n"
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

    {"/proc/meminfo",
        "MemTotal:         131072 kB\n"
        "MemFree:          129024 kB\n"
        "MemAvailable:     128000 kB\n"
        "Buffers:          2048 kB\n"
        "Cached:           2048 kB\n",
        130, 0},

    {"/proc/version",
        "Galio version 0.1.0 (built for x86 32-bit)\n",
        46, 0},

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

    {"/var/log/dmesg",
        "[    0.000000] Linux version (built for x86 32-bit)\n"
        "[    0.000000] Command line: \n"
        "[    0.000000] BIOS-provided physical RAM map:\n"
        "[    0.000000] BIOS-e820: [mem 0x0000000000000000-0x000000000009fbff] usable\n",
        230, 0},

    /* User home files */
    {"/home/Desktop/readme.txt",
        "Desktop Directory\n"
        "=================\n"
        "\n"
        "This is your desktop directory.\n"
        "Place your shortcuts and files here.\n",
        80, 0},

    {"/root/.bashrc",
        "# Root's bash configuration\n"
        "export PS1='root@galio:~# '\n"
        "export EDITOR=vi\n"
        "alias ls='ls --color=auto'\n",
        100, 0},

    {"/root/.profile",
        "# Root's profile\n"
        "if [ -f ~/.bashrc ]; then\n"
        "    . ~/.bashrc\n"
        "fi\n",
        60, 0},

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

    {"/sbin/init",
        "#!/bin/galio\n"
        "# System init script\n"
        "# Root filesystem initialization\n",
        57, 0},

    {"/sbin/shutdown",
        "#!/bin/sh\n"
        "# Shutdown script\n"
        "echo 'Shutting down system...'\n",
        54, 0},

    {"/sbin/reboot",
        "#!/bin/sh\n"
        "# Reboot script\n"
        "echo 'Rebooting system...'\n",
        50, 0},

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

    {"/usr/share/doc/README",
        "Galio Operating System\n"
        "========================\n"
        "\n"
        "A lightweight 32-bit kernel for educational purposes.\n"
        "Documentation can be found in /usr/share/doc/\n",
        150, 0},

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

    {"/etc/init.d/ssh",
        "#!/bin/sh\n"
        "# SSH daemon initialization script\n"
        "echo 'Starting SSH daemon...'\n",
        66, 0},

    {"/etc/init.d/cron",
        "#!/bin/sh\n"
        "# Cron daemon initialization script\n"
        "echo 'Starting cron service...'\n",
        67, 0},

    /* Device information */
    {"/etc/udev/udev.conf",
        "# /etc/udev/udev.conf - udev configuration\n"
        "# Runs programs to completion by default\n"
        "udev_log=\"info\"\n"
        "udev_rules=\"/etc/udev/rules.d/\"\n",
        150, 0},

    /* Modprobe configuration */
    {"/etc/modprobe.d/blacklist.conf",
        "# /etc/modprobe.d/blacklist.conf - blacklisted kernel modules\n"
        "# blacklist floppy\n"
        "# blacklist pcspkr\n",
        100, 0},

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

    /* Build a current initrd boot config with a runtime boot_time value. */
    {
        char boot_time_string[32];
        build_boot_time_string(boot_time_string, sizeof(boot_time_string));
        snprintf(boot_config_txt, sizeof(boot_config_txt),
                 "kernel=galio\n"
                 "version=0.1.0\n"
                 "arch=x86-32\n"
                 "bootloader=GRUB\n"
                 "boot_time=%s\n",
                 boot_time_string);
        for (int i = 0; i < file_count; i++) {
            if (strcmp(files[i].path, "/boot/config.txt") == 0) {
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