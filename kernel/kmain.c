#include "vga.h"
#include "framebuffer.h"
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "kprintf.h"
#include "display/display.h"
#include "panel/panel.h"
#include "mouse/cursor.h"
#include "serial.h"
#include "pmem.h"
#include "paging.h"
#include "heap.h"
#include "pit.h"
#include "keyboard.h"
#include "process.h"
#include "vfs.h"
#include "dev/device_manager.h"
#include "vfs_core.h"
#include "ata.h"
#include "ext2.h"
#include "init.h"
#include "cpu/cpu.h"
#include "cpu/scheduler.h"
#include "cpufreq/cpufreq.h"
#include "auth.h"
#include "string.h"
#include <string.h>
#include "kernel_time.h"
#include "pci.h"
#include "net/net.h"
#include "net/wifi.h"
#include "drivers/net/e1000.h"
#include "drivers/net/rtl8188eu.h"
#include "power/power.h"
#include "shell.h"

// Disk entry - line: 193

/* Syscall interface declaration */
void syscall_init(void);

/* Memory test declaration */
void mem_test_run(void);

/* Kernel test declaration */
void run_kernel_tests(void);

/* Embedded test binary */
extern u8 _binary_test_elf_bin_start;
extern u8 _binary_test_elf_bin_end;

/* Populate disk with initrd contents on first boot */
static void vfs_populate_disk_from_initrd(void) {
    if (!vfs_core_is_disk_mode()) return;
    
    /* List of directories to create from initrd */
    const char *dirs[] = {
        /* Core directories */
        "./boot", "./bin", "./sbin", "./dev", "./etc", "./usr/home", "./lib", 
        "./mnt", "./media", "./proc", "./root", "./run", "./srv", "./sys", "./tmp", 
        "./fuse", "./lost+found",
        
        /* /usr subdirectories */
        "./usr", "./usr/bin", "./usr/sbin", "./usr/lib", "./usr/local", 
        "./usr/local/bin", "./usr/local/sbin", "./usr/local/lib", 
        "./usr/local/share", "./usr/share", "./usr/share/doc", 
        "./usr/share/man", "./usr/share/info", "./usr/include", 
        "./usr/src", "./usr/games", "./usr/libexec",
        
        /* /var subdirectories */
        "./var", "./var/log", "./var/run", "./var/spool", "./var/spool/cron", 
        "./var/spool/mail", "./var/crash", "./var/lock", "./var/account", 
        "./var/mail", "./var/tmp", "./var/cache", "./var/cache/apt", "./var/games",
        
        /* /etc subdirectories */
        "./etc/X11", "./etc/X11/xorg.conf.d", "./etc/opt", "./etc/sgml", 
        "./etc/init.d", "./etc/rc.d", "./etc/rc.d/init.d", "./etc/share", 
        "./etc/xml", "./etc/ssl", "./etc/ssl/certs", "./etc/ssl/private", 
        "./etc/skel", "./etc/pam.d", "./etc/sysconfig", "./etc/cron.d", 
        "./etc/cron.daily", "./etc/cron.weekly", "./etc/cron.monthly", 
        "./etc/cron.hourly", "./etc/security", "./etc/selinux", "./etc/iptables", 
        "./etc/network", "./etc/network/if-up.d", "./etc/network/if-down.d", 
        "./etc/network/if-pre-up.d", "./etc/network/if-post-down.d", 
        "./etc/profile.d", "./etc/modprobe.d", "./etc/ssh",
        
        /* /opt subdirectories */
        "./opt", "./opt/bin", "./opt/sbin", "./opt/etc", "./opt/var", 
        "./opt/lib", "./opt/share", "./opt/share/doc", "./opt/share/man", 
        "./opt/local", "./opt/src",
        
        /* /home subdirectories (moved under /usr) */
        "./usr/home/desktop", "./usr/home/desktop/recycle", "./usr/home/downloads",
        "./usr/home/music", "./usr/home/documents", "./usr/home/videos", "./usr/home/recent",
        "./usr/home/images", "./usr/home/pictures", 
        
        NULL
    };
    
    /* Create directories on disk */
    for (int i = 0; dirs[i] != NULL; i++) {
        u8 ok = vfs_core_create_dir(dirs[i], 1);
        if (!ok) {
            kprintf("  Create FAILED: %s\n", dirs[i]);
        }
        if (!vfs_is_dir(dirs[i])) {
            kprintf("  VFS verify FAILED for dir: %s\n", dirs[i]);
        }
    }
    
    /* Copy files from initrd to disk */
    extern u8 _binary_initrd_bin_start;
    vfs_header_t *header = (vfs_header_t *)&_binary_initrd_bin_start;

    for (u32 i = 0; i < header->entry_count; i++) {
        vfs_entry_t *entry = &header->entries[i];
        if (!entry->is_dir && entry->size > 0) {
            /* Skip files that already exist on disk to preserve user data */
            if (ext2_find_inode(entry->path) != 0) {
                continue;
            }

            /* Read file content from initrd */
            u8 *content = (u8 *)header + entry->offset;
            
            /* Write file to disk */
            if (vfs_core_create_file(entry->path, 0)) {
                u32 inode = ext2_find_inode(entry->path);
                if (inode) {
                    ext2_write_data(inode, content, entry->size);
                } else {
                    kprintf("  Created but inode lookup failed: %s\n", entry->path);
                }
            }
        }
    }
}

static void vfs_ensure_home_dirs(void) {
    const char *dirs[] = {
        "./usr/home", "./usr/home/desktop", "./usr/home/desktop/recycle", "./usr/home/downloads",
        "./usr/home/music", "./usr/home/documents", "./usr/home/videos", "./usr/home/recent",
        "./usr/home/images", "./usr/home/pictures", NULL
    };

    for (int i = 0; dirs[i] != NULL; i++) {
        if (!vfs_mkdir(dirs[i], 1)) {
            kprintf("[VFS] WARNING: Could not create RAM dir: %s\n", dirs[i]);
        }
    }
}

static void vfs_verify_disk_write(void) {
    if (!vfs_core_is_disk_mode()) return;

    const char *path = "./disk_verify_test.txt";
    const char *payload = "Galio disk write verification\n";
    u32 inode_num = ext2_find_inode(path);
    if (inode_num == 0) {
        i32 created = ext2_create_file(path, 0x81A4);
        if (created < 0) {
            kprintf("[VFS] Disk verification failed: unable to create %s\n", path);
            return;
        }
        inode_num = (u32)created;
    }

    i32 written = ext2_write_data(inode_num, payload, strlen(payload));
    if (written < 0) {
        kprintf("[VFS] Disk verification failed: ext2_write_data returned %d\n", written);
        return;
    }

    char buffer[64];
    memset(buffer, 0, sizeof(buffer));
    i32 read = ext2_read_data(inode_num, buffer, written, 0);
    if (read != written || strncmp(buffer, payload, written) != 0) {
        kprintf("[VFS] Disk verification failed: read back %d bytes, expected %u\n", read, written);
        return;
    }
}

static void register_kernel_services(void) {
    static const char *const services[] = {
        "/kernel/scheduler",
        "/kernel/timer",
        "/kernel/interrupts",
        "/kernel/memory",
        "/kernel/vfs",
        "/kernel/ext2",
        "/kernel/devices",
        "/kernel/syscalls",
        "/kernel/ipc",
        "/kernel/security",
        "/kernel/reaper",
        "/kernel/kworker",
        "/kernel/ksoftirqd",
        "/kernel/kblockd",
        "/kernel/kswapd",
        "/sbin/devd",
        "/sbin/logd",
        "/sbin/netd",
        NULL
    };

    for (u32 i = 0; services[i] != NULL; i++) {
        if (!process_create_kernel_service(services[i])) {
            kprintf("Failed to register kernel service: %s\n", services[i]);
        }
    }
}

/* Entry point from bootloader - receives Multiboot info */
void kmain(void *multiboot_ptr) {
    (void)multiboot_ptr;

    serial_init();
    vga_init();
    fb_init();
    vga_set_color(0x0A);
    kprintf("=== Galio Kernel Boot ===\n\n");

    kprintf("Initializing VGA...\n");

    kprintf("Initializing GDT...\n");
    gdt_init();

    kprintf("Initializing IDT...\n");
    idt_init();

    kprintf("Installing IRQ handlers...\n");
    irq_install();

    kprintf("Initializing physical memory manager...\n");

    typedef struct {
        u32 flags;
        u32 mem_lower;
        u32 mem_upper;
        u32 boot_device;
        u32 cmdline;
        u32 mods_count;
        u32 mods_addr;
        u32 syms[4];
        u32 mmap_length;
        u32 mmap_addr;
    } multiboot_info_t;

    multiboot_info_t *mb_info = (multiboot_info_t *)multiboot_ptr;
    u32 mmap_addr = 0;
    u32 mmap_length = 0;

    if (mb_info && (mb_info->flags & (1 << 6))) {
        mmap_addr = mb_info->mmap_addr;
        mmap_length = mb_info->mmap_length;
        kprintf("Found Multiboot mmap: addr=%x len=%u\n", mmap_addr, mmap_length);
    } else {
        kprintf("No Multiboot mmap available, using fallback\n");
    }

    pmem_init(mmap_addr, mmap_length);

    kprintf("Initializing paging...\n");
    paging_init();
    kprintf("Initializing heap...\n");
    heap_init();

    /* UI shell is intentionally disabled; the system boots directly into fullscreen gsh after auth. */

    kprintf("Initializing networking subsystem...\n");
    net_init();
    wifi_init();
    rtl8188eu_register_driver();
    e1000_register_driver();
    pci_init();
    net_print_devices();

    kprintf("Running memory stabilization tests...\n");
    mem_test_run();

    kprintf("Initializing process manager...\n");
    process_init();

    kprintf("Initializing power subsystem...\n");
    power_suspend_init();
    kprintf("[POWER] self-test: reset=ready, shutdown=ready, suspend=ready\n");

    kprintf("Installing system call handler...\n");
    syscall_init();

    kprintf("Initializing timer (100 Hz)...\n");
    pit_init(100);
    /* Initialize wall-clock from CMOS/RTC if available */
    kernel_time_initialize();

    kprintf("Initializing CPU subsystem...\n");
    cpu_init();
    cpufreq_init();

    kprintf("Initializing scheduler...\n");
    cpu_scheduler_init();

    kprintf("Initializing keyboard...\n");
    keyboard_init();

    kprintf("Initializing filesystem...\n");

    extern u8 _binary_initrd_bin_start;
    vfs_init(&_binary_initrd_bin_start);
    if (device_manager_init() != 0) {
        kprintf("[DEV] ERROR: Device subsystem initialization failed\n");
    }
    /* Attempt to read boot wall-clock time from the hidden boot config and set it */
    {
        char cfg[128];
        for (u32 i = 0; i < sizeof(cfg); i++) cfg[i] = 0;
        u32 r = vfs_read("./boot/.config.txt", cfg, sizeof(cfg) - 1);
        if (r > 0) {
            kprintf("[TIME] Loaded ./boot/.config.txt (%u bytes)\n", r);
            const char *key = "boot_time=";
            char *p = NULL;
            for (u32 i = 0; cfg[i]; i++) {
                if (strncmp(&cfg[i], key, strlen(key)) == 0) {
                    p = &cfg[i + strlen(key)];
                    break;
                }
            }
            if (p) {
                char datebuf[32];
                u32 di = 0;
                while (*p && *p != '\n' && di + 1 < sizeof(datebuf)) {
                    datebuf[di++] = *p++;
                }
                datebuf[di] = '\0';
                u32 epoch = kernel_time_parse_yyyy_mm_dd_to_epoch(datebuf);
                if (epoch) {
                    DateTime current_time = kernel_time_get_datetime();
                    if (current_time.year < 1970) {
                        kernel_time_set_boot_seconds(epoch);
                        kprintf("[TIME] Boot time set from config: %s (epoch=%u)\n", datebuf, epoch);
                    } else {
                        kprintf("[TIME] RTC valid; ignoring stale boot_time config: %s\n", datebuf);
                    }
                }
            }

            const char *tz_key = "timezone_offset_hours=";
            char *tz = NULL;
            for (u32 i = 0; cfg[i]; i++) {
                if (strncmp(&cfg[i], tz_key, strlen(tz_key)) == 0) {
                    tz = &cfg[i + strlen(tz_key)];
                    break;
                }
            }
            bool tz_found = false;
            if (tz) {
                tz_found = true;
                int sign = 1;
                if (*tz == '+') {
                    tz++;
                } else if (*tz == '-') {
                    sign = -1;
                    tz++;
                }
                int offset = 0;
                if (!(*tz >= '0' && *tz <= '9')) {
                    kprintf("[TIME] Invalid timezone_offset_hours value\n");
                } else {
                    while (*tz >= '0' && *tz <= '9') {
                        offset = offset * 10 + (*tz - '0');
                        tz++;
                    }
                    offset *= sign;
                    if (offset < -24 || offset > 24) {
                        kprintf("[TIME] timezone_offset_hours out of range: %d\n", offset);
                    } else {
                        s32 offset_seconds = (s32)offset * 3600;
                        kernel_time_set_timezone_offset_seconds(offset_seconds);
                        const char *sign_str = (offset < 0) ? "-" : "+";
                        kprintf("[TIME] Timezone offset configured: %s%u hours\n", sign_str, (offset < 0) ? (u32)(-offset) : (u32)offset);
                    }
                }
            }
            if (!tz_found) {
                kprintf("[TIME] No timezone_offset_hours config found; defaulting to +6 hours (Bangladesh)\n");
            }
        } else {
            kprintf("[TIME] ./boot/.config.txt not found or empty; using default timezone offset +6 hours (Bangladesh)\n");
        }
    }
    vfs_ensure_home_dirs();
    vfs_debug();

    kprintf("Running kernel self-tests...\n");
    run_kernel_tests();

    kprintf("Initializing ATA driver...\n");
    ata_init();

    /* Persistent disk image is a raw EXT2 filesystem at LBA 0 (see iso.sh) */
    ext2_set_partition_lba(0);

    if (ext2_init() == 0) {
        kprintf("EXT2 partition mounted successfully\n");

        /* Enable disk-backed filesystem */
        extern void vfs_core_init_disk_mode(void);
        extern u8 vfs_core_reload_root_from_disk(void);

        vfs_core_init_disk_mode();

        /* Initialize memory/disk persistence plumbing */
        extern void memory_init_disk_persistence(void);
        memory_init_disk_persistence();

        /* Always populate disk from initrd and verify writes.
           Reloading root dentry can fail without breaking disk writes themselves,
           and we still want shell-visible filesystem content. */
        if (!vfs_core_reload_root_from_disk()) {
            kprintf("[VFS] Warning: failed to reload disk root (shell may still work with disk mode)\n");
        }
        vfs_populate_disk_from_initrd();  /* copy initrd contents into disk only when missing */
        vfs_verify_disk_write();          /* verify disk write/read */
    } else {
        kprintf("No EXT2 partition found, using RAM filesystem only\n");
    }

    auth_bootstrap();

    /* Register init before the interactive shell so top can see it. */
    u32 init_pid = process_create(init_main, 1);
    if (!init_pid) {
        kprintf("Failed to create init process!\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
    process_set_path(process_get(init_pid), "/sbin/init");
    register_kernel_services();

    /* The shell runs in the boot process context. Enable timer and keyboard
     * interrupts before entering it so scheduling and CPU accounting work. */
    __asm__ volatile("sti");
    irq_unmask(1);

    display_enter_shell_mode();
    process_set_path(process_current(), "/bin/gsh");
    enable_interrupts();
    shell_run();

    for (;;) {
        __asm__ volatile("hlt");
    }
}