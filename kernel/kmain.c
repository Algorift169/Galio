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

// Disk entry - line: 193

/* Syscall interface declaration */
void syscall_init(void);

/* Memory test declaration */
void mem_test_run(void);

/* Embedded test binary */
extern u8 _binary_test_elf_bin_start;
extern u8 _binary_test_elf_bin_end;

/* Populate disk with initrd contents on first boot */
static void vfs_populate_disk_from_initrd(void) {
    if (!vfs_core_is_disk_mode()) return;
    
    /* List of directories to create from initrd */
    const char *dirs[] = {
        "/boot", "/bin", "/etc", "/lib", "/dev", "/tmp",
        "/home", "/home/desktop", "/home/desktop/recycle", "/home/downloads",
        "/home/music", "/home/doccuments", "/home/videos", "/home/recent",
        "/usr", "/proc", "/sys", "/var", "/var/log",
        "/usr/bin", "/usr/lib", "/etc/init.d", NULL
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
        "/home/music", "/home/doccuments", "/home/videos", "/home/recent", NULL
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

    /* FILESYSTEM TESTS DISABLED - they cause crashes in disk mode */
    // kprintf("\n");
    // kprintf("╔══════════════════════════════════════════════════════════════╗\n");
    // kprintf("║           FILESYSTEM VERIFICATION & DEBUG OUTPUT             ║\n");
    // kprintf("╚══════════════════════════════════════════════════════════════╝\n");
    // kprintf("\n");
    // 
    // kprintf("[TEST 1] Filesystem Statistics\n");
    // kprintf("─────────────────────────────────────────────────────────────\n");
    // vfs_stats();
    // kprintf("\n");
    // 
    // kprintf("[TEST 2] Root Directory Contents\n");
    // kprintf("─────────────────────────────────────────────────────────────\n");
    // vfs_listdir("/");
    // kprintf("\n");
    // 
    // kprintf("[TEST 3] Reading /etc/hostname\n");
    // kprintf("─────────────────────────────────────────────────────────────\n");
    // char hostname_buf[256];
    // u32 bytes_read = vfs_read("/etc/hostname", hostname_buf, 255);
    // if (bytes_read > 0) {
    //     hostname_buf[bytes_read] = 0;
    //     kprintf("Successfully read %u bytes\n", bytes_read);
    //     kprintf("  Content: %s", hostname_buf);
    // } else {
    //     kprintf("FAILED to read /etc/hostname\n");
    // }
    // kprintf("\n");
    // 
    // kprintf("[TEST 4] Reading /boot/config.txt\n");
    // kprintf("─────────────────────────────────────────────────────────────\n");
    // char config_buf[512];
    // bytes_read = vfs_read("/boot/config.txt", config_buf, 511);
    // if (bytes_read > 0) {
    //     config_buf[bytes_read] = 0;
    //     kprintf("Successfully read %u bytes\n", bytes_read);
    //     kprintf("  Content:\n%s", config_buf);
    // } else {
    //     kprintf("FAILED to read /boot/config.txt\n");
    // }
    // kprintf("\n");
    // kprintf("Filesystem tests completed.\n");
    // kprintf("─────────────────────────────────────────────────────────────\n");

    auth_bootstrap();

    vga_clear();
    if (kernel_auth.registered) {
        kprintf("Welcome \"%s\" to Galio !\n", kernel_auth.username);
    } else {
        kprintf("Welcome to Galio !\n");
    }
    kprintf("Press c to enter GSH....\n\n");

    /* ELF test disabled - needs proper kernel virtual address mapping */
    // kprintf("Loading ELF loader smoke test...\n");
    // u32 elf_entry = elf_load(&_binary_test_elf_bin_start);
    // if (elf_entry) {
    //     kprintf("ELF loaded successfully, entry point: %08X\n", elf_entry);
    //     kprintf("Executing test ELF...\n");
    //     ((void (*)(void))elf_entry)();
    // } else {
    //     kprintf("ELF_LOADER_TEST: FAILED\n");
    // }

    /* Disable all interrupts while polling the keyboard manually */
    __asm__ volatile("cli");

    /* Mask all IRQs on both PICs */
    outb(0x21, 0xFF);  /* Master PIC */
    outb(0xA1, 0xFF);  /* Slave PIC */

    /* Poll for 'c' key press using direct port I/O */
    while (1) {
        u8 status = inb(0x64);
        
        if (status & 0x01) {
            u8 scancode = inb(0x60);
            
            if (scancode == 0x2E) {  /* 'c' key */
                /* Drain keyboard buffer */
                while (inb(0x64) & 0x01) {
                    inb(0x60);
                }
                kprintf("\n'c' key detected! Launching shell...\n\n");
                break;
            }
        }
        
        /* Small delay to avoid busy loop */
        for (volatile int i = 0; i < 1000; i++);
    }

    extern void shell_run(void);

    /* Enable interrupts so IRQ-driven keyboard events can be delivered to the shell. */
    __asm__ volatile("sti");
    shell_run();

    /* shell_run should not return. */
    for (;;) {
        __asm__ volatile("hlt");
    }

    /* Restore timer IRQ so the scheduler can run once interrupts are enabled */
    irq_unmask(0);

    /* Create init process */
    kprintf("Creating init process...\n");
    u32 init_pid = process_create(init_main, 1);
    if (!init_pid) {
        kprintf("Failed to create init process!\n");
        for(;;);
    }

    process_set_boot_current();

    /* Switch to idle process to start multitasking */
    kprintf("Kernel initialization complete. Starting multitasking.\n");
    process_yield();

    /* Enable interrupts after first yield */
    __asm__ volatile("sti");

    /* Should never reach here */
    kprintf("ERROR: Returned from multitasking start!\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}