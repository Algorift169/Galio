#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "kprintf.h"
#include "serial.h"
#include "pmem.h"
#include "paging.h"
#include "heap.h"
#include "pit.h"
#include "keyboard.h"
#include "process.h"
#include "vfs.h"
#include "vfs_core.h"
#include "ata.h"
#include "ext2.h"
#include "init.h"
#include "cpu.h"
#include "scheduler.h"
#include "auth.h"
#include "string.h"
#include "pci.h"
#include "net/net.h"
#include "net/wifi.h"
#include "drivers/net/e1000.h"

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
        "/boot", "/bin", "/sbin", "/dev", "/etc", "/home", "/lib", 
        "/mnt", "/media", "/proc", "/root", "/run", "/srv", "/sys", "/tmp", 
        "/fuse", "/lost+found",
        
        /* /usr subdirectories */
        "/usr", "/usr/bin", "/usr/sbin", "/usr/lib", "/usr/local", 
        "/usr/local/bin", "/usr/local/sbin", "/usr/local/lib", 
        "/usr/local/share", "/usr/share", "/usr/share/doc", 
        "/usr/share/man", "/usr/share/info", "/usr/include", 
        "/usr/src", "/usr/games", "/usr/libexec",
        
        /* /var subdirectories */
        "/var", "/var/log", "/var/run", "/var/spool", "/var/spool/cron", 
        "/var/spool/mail", "/var/crash", "/var/lock", "/var/account", 
        "/var/mail", "/var/tmp", "/var/cache", "/var/cache/apt", "/var/games",
        
        /* /etc subdirectories */
        "/etc/X11", "/etc/X11/xorg.conf.d", "/etc/opt", "/etc/sgml", 
        "/etc/init.d", "/etc/rc.d", "/etc/rc.d/init.d", "/etc/share", 
        "/etc/xml", "/etc/ssl", "/etc/ssl/certs", "/etc/ssl/private", 
        "/etc/skel", "/etc/pam.d", "/etc/sysconfig", "/etc/cron.d", 
        "/etc/cron.daily", "/etc/cron.weekly", "/etc/cron.monthly", 
        "/etc/cron.hourly", "/etc/security", "/etc/selinux", "/etc/iptables", 
        "/etc/network", "/etc/network/if-up.d", "/etc/network/if-down.d", 
        "/etc/network/if-pre-up.d", "/etc/network/if-post-down.d", 
        "/etc/profile.d", "/etc/modprobe.d", "/etc/ssh",
        
        /* /opt subdirectories */
        "/opt", "/opt/bin", "/opt/sbin", "/opt/etc", "/opt/var", 
        "/opt/lib", "/opt/share", "/opt/share/doc", "/opt/share/man", 
        "/opt/local", "/opt/src",
        
        /* /home subdirectories */
        "/home/desktop", "/home/desktop/recycle", "/home/downloads",
        "/home/music", "/home/documents", "/home/videos", "/home/recent",
        "/home/images", "/home/pictures", 
        
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
        "/home", "/home/desktop", "/home/desktop/recycle", "/home/downloads",
        "/home/music", "/home/documents", "/home/videos", "/home/recent",
        "/home/images", "/home/pictures", NULL
    };

    for (int i = 0; dirs[i] != NULL; i++) {
        if (!vfs_mkdir(dirs[i], 1)) {
            kprintf("[VFS] WARNING: Could not create RAM dir: %s\n", dirs[i]);
        }
    }
}

static void vfs_verify_disk_write(void) {
    if (!vfs_core_is_disk_mode()) return;

    const char *path = "/disk_verify_test.txt";
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

/* Entry point from bootloader - receives Multiboot info */
void kmain(void *multiboot_ptr) {
    (void)multiboot_ptr;

    serial_init();
    kprintf("=== Galio Kernel Boot ===\n\n");

    kprintf("Initializing VGA...\n");
    vga_init();

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

    kprintf("Initializing networking subsystem...\n");
    net_init();
    wifi_init();
    e1000_register_driver();
    pci_init();
    net_print_devices();

    kprintf("Running memory stabilization tests...\n");
    mem_test_run();

    kprintf("Initializing process manager...\n");
    process_init();

    kprintf("Installing system call handler...\n");
    syscall_init();

    kprintf("Initializing timer (100 Hz)...\n");
    pit_init(100);

    kprintf("Initializing scheduler...\n");
    scheduler_init();

    kprintf("Initializing keyboard...\n");
    keyboard_init();

    kprintf("Initializing filesystem...\n");

    extern u8 _binary_initrd_bin_start;
    vfs_init(&_binary_initrd_bin_start);
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

    vga_clear();
    if (kernel_auth.registered) {
        kprintf("Welcome \"%s\" to Galio !\n", kernel_auth.username);
    } else {
        kprintf("Welcome to Galio !\n");
    }
    kprintf("Press 1 to enter GSH (Shell)\n");
    kprintf("Press 2 to enter Cursor Movement Mode\n\n");

    /* Create init process early so the embedded ELF gets scheduled */
    u32 init_pid = process_create(init_main, 1);
    if (!init_pid) {
        kprintf("Failed to create init process!\n");
        for(;;);
    }

    /* Keep interrupts enabled for keyboard input */
    __asm__ volatile("sti");
    
    /* Unmask keyboard IRQ */
    irq_unmask(1);

    int selected_mode = 0;
    kprintf("Waiting for input...\n");

    while (selected_mode == 0) {
        u8 scancode = 0;
        u8 is_pressed = 0;
        u8 extended = 0;

        if (keyboard_read_event(&scancode, &is_pressed, &extended) && is_pressed && !extended) {
            u8 raw = scancode & 0x7F;
            if (raw == 0x02) {  /* '1' key */
                selected_mode = 1;
                kprintf("\nSelected: GSH Shell\n\n");
            } else if (raw == 0x03) {  /* '2' key */
                selected_mode = 2;
                kprintf("\nSelected: Cursor Movement Mode\n\n");
            }
        }
        
        /* Small delay to prevent CPU spinning */
        for (volatile int i = 0; i < 1000; i++);
    }

    extern void shell_run(void);

    if (selected_mode == 1) {
        /* Enter shell mode */
        shell_run();
        for (;;) {
            __asm__ volatile("hlt");
        }
    } else if (selected_mode == 2) {
        /* Enter cursor movement mode - move the VGA hardware cursor */
        vga_clear();
        //kprintf("=== VGA CURSOR MOVEMENT MODE ===\n");
        //kprintf("Use ARROW KEYS to move the blinking cursor\n");
        //kprintf("Press ESC to exit to kernel panic\n\n");
        //kprintf("Current cursor position: ");
        
        /* Get current cursor position */
        int cursor_x = 40, cursor_y = 12;
        vga_move_hardware_cursor(cursor_x, cursor_y);
        
        int last_x = -1, last_y = -1;
        
        for (;;) {
            u8 scancode = 0;
            u8 is_pressed = 0;
            u8 extended = 0;
            
            if (keyboard_read_event(&scancode, &is_pressed, &extended) && is_pressed) {
                u8 raw = scancode & 0x7F;
                
                /* Handle arrow keys, whether extended or not */
                if (extended || raw == 0x48 || raw == 0x50 || raw == 0x4B || raw == 0x4D) {
                    switch (raw) {
                        case 0x48:  /* Up arrow */
                            cursor_y--;
                            if (cursor_y < 0) cursor_y = 0;
                            vga_move_hardware_cursor(cursor_x, cursor_y);
                            break;
                        case 0x50:  /* Down arrow */
                            cursor_y++;
                            if (cursor_y >= 25) cursor_y = 24;
                            vga_move_hardware_cursor(cursor_x, cursor_y);
                            break;
                        case 0x4B:  /* Left arrow */
                            cursor_x--;
                            if (cursor_x < 0) cursor_x = 0;
                            vga_move_hardware_cursor(cursor_x, cursor_y);
                            break;
                        case 0x4D:  /* Right arrow */
                            cursor_x++;
                            if (cursor_x >= 80) cursor_x = 79;
                            vga_move_hardware_cursor(cursor_x, cursor_y);
                            break;
                    }
                }
                
                /* ESC key to exit */
                if (!extended && raw == 0x01) {
                    //kprintf("\n\nESC pressed - exiting\n");
                    break;
                }
            }
            
            /* Update position display every 50ms */
            static int counter = 0;
            counter++;
            if (counter >= 50 && (cursor_x != last_x || cursor_y != last_y)) {
                /* Save cursor position */
                int old_x, old_y;
                vga_get_hardware_cursor(&old_x, &old_y);
                
                /* Print at fixed position */
                vga_move_hardware_cursor(25, 2);
                //kprintf("(%d, %d)  ", cursor_x, cursor_y);
                
                /* Restore cursor */
                vga_move_hardware_cursor(old_x, old_y);
                
                last_x = cursor_x;
                last_y = cursor_y;
                counter = 0;
            }
            
            for (volatile int i = 0; i < 100; i++);
            __asm__ volatile("hlt");
        }
        
        vga_clear();
        //kprintf("Exited cursor movement mode. Halting.\n");
        for (;;) __asm__ volatile("hlt");
    }

    /* Should never reach here */
    for (;;) {
        __asm__ volatile("hlt");
    }
}