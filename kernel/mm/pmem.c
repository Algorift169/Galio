/* pmem.c - Physical memory manager */
#include "pmem.h"
#include "kprintf.h"

#define FRAME_SIZE 4096
#define FRAMES_PER_BYTE 8
#define BITMAP_SIZE (128 * 1024 * 1024 / FRAME_SIZE / 8)  /* For 128MB */

static u8 frame_bitmap[BITMAP_SIZE] = {0};
static u8 frame_refcount[BITMAP_SIZE * 8] = {0};
static u32 total_frames = 0;
static u32 used_frames = 0;
static u32 kernel_frames = 0;

#define FRAME_MASK(frame) ((frame) / 8)
#define BIT_MASK(frame)   (1 << ((frame) % 8))

extern void process_oom_kill(void);

static void set_frame(u32 frame) {
    frame_bitmap[FRAME_MASK(frame)] |= BIT_MASK(frame);
}

static void unset_frame(u32 frame) {
    frame_bitmap[FRAME_MASK(frame)] &= ~BIT_MASK(frame);
}

static u8 get_frame(u32 frame) {
    return frame_bitmap[FRAME_MASK(frame)] & BIT_MASK(frame);
}

static void ref_frame(u32 frame) {
    if (frame_refcount[frame] == 0) {
        set_frame(frame);
        used_frames++;
    }
    frame_refcount[frame]++;
}

static void deref_frame(u32 frame) {
    if (frame_refcount[frame] == 0) return;
    frame_refcount[frame]--;
    if (frame_refcount[frame] == 0) {
        unset_frame(frame);
        used_frames--;
    }
}

void pmem_init(u32 mmap_addr, u32 mmap_length) {
    u32 kernel_end = 0x400000;  /* Approximate kernel end */

    kprintf("Physical memory manager initializing...\n");
    kprintf("pmem_init: Bitmap size = %u bytes (supports %u MB)\n", BITMAP_SIZE, BITMAP_SIZE * 8 * FRAME_SIZE / (1024*1024));

    /* Mark all memory as used initially */
    for (u32 frame = 0; frame < BITMAP_SIZE * 8; frame++) {
        set_frame(frame);
    }

    /* If no valid mmap, assume 128MB is available */
    if (mmap_addr == 0 || mmap_length == 0) {
        kprintf("pmem_init: No Multiboot memory map, assuming 128 MB available\n");

        /* Assume memory from 0x100000 to 0x8000000 is available (126 MB) */
        u32 start_frame = 0x100000 / FRAME_SIZE;
        u32 end_frame = 0x8000000 / FRAME_SIZE;

        kprintf("pmem_init: Marking frames %u-%u as available (0x100000-0x8000000)\n", start_frame, end_frame);

        for (u32 frame = start_frame; frame < end_frame; frame++) {
            unset_frame(frame);
            total_frames++;
        }
    } else {
        kprintf("pmem_init: Found Multiboot mmap: addr=%x, len=%u\n", mmap_addr, mmap_length);

        /* Parse Multiboot memory map */
        mmap_entry_t *entry = (mmap_entry_t *)mmap_addr;
        u32 entry_count = 0;

        while ((u32)entry < mmap_addr + mmap_length) {
            u32 addr = entry->addr_low;
            u32 len = entry->len_low;
            u32 next = (u32)entry + entry->size + sizeof(entry->size);

            if (entry->type == MMAP_AVAILABLE && len > 0) {
                u32 start_frame = addr / FRAME_SIZE;
                u32 end_frame = (addr + len) / FRAME_SIZE;

                kprintf("pmem_init: Available region %u: %x-%x (%u KB)\n",
                        entry_count, addr, addr + len, len / 1024);

                for (u32 frame = start_frame; frame < end_frame; frame++) {
                    unset_frame(frame);
                    total_frames++;
                }
            } else {
                kprintf("pmem_init: Reserved region %u: %x-%x (type=%u)\n",
                        entry_count, addr, addr + len, entry->type);
            }

            entry = (mmap_entry_t *)next;
            entry_count++;
        }

        kprintf("pmem_init: Parsed %u memory map entries\n", entry_count);
    }

    /* Mark kernel space as used */
    u32 kernel_start_frame = 0x100000 / FRAME_SIZE;
    u32 kernel_end_frame = kernel_end / FRAME_SIZE;

    kprintf("pmem_init: Marking kernel frames %u-%u (0x100000-0x%x) as used\n",
            kernel_start_frame, kernel_end_frame, kernel_end);

    for (u32 frame = kernel_start_frame; frame < kernel_end_frame; frame++) {
        if (frame_refcount[frame] == 0) {
            ref_frame(frame);
            kernel_frames++;
        }
    }

    kprintf("pmem_init: Status: total=%u frames (%u MB), kernel=%u frames (%u KB), free=%u frames (%u MB)\n",
            total_frames, total_frames * FRAME_SIZE / (1024 * 1024),
            kernel_frames, kernel_frames * FRAME_SIZE / 1024,
            (total_frames - used_frames), (total_frames - used_frames) * FRAME_SIZE / (1024 * 1024));

    /* Mark frame 0 and low memory regions (0-0x100000) as used to avoid conflicts */
    for (u32 frame = 0; frame < 0x100000 / FRAME_SIZE; frame++) {
        if (frame_refcount[frame] == 0) {
            ref_frame(frame);
        }
    }
}

u32 pmem_alloc(size_t num_frames) {
    if (num_frames == 0) {
        kprintf("pmem_alloc: Cannot allocate 0 frames\n");
        return 0;
    }

    if ((total_frames - used_frames) < num_frames) {
        process_oom_kill();
    }

    for (u32 frame = 0; frame < BITMAP_SIZE * 8; frame++) {
        if (!get_frame(frame)) {
            /* Check if we have enough contiguous frames */
            u8 found = 1;
            for (u32 i = 1; i < num_frames; i++) {
                if (get_frame(frame + i)) {
                    found = 0;
                    break;
                }
            }

            if (found) {
                u32 addr = frame * FRAME_SIZE;
                for (u32 i = 0; i < num_frames; i++) {
                    ref_frame(frame + i);
                }
                kprintf("pmem_alloc: Allocated %u frame(s) at addr=%x (frame %u)\n", num_frames, addr, frame);
                return addr;
            }
        }
    }

    kprintf("pmem_alloc: Out of physical memory (requested %u frames, have %u free)\n",
            num_frames, total_frames - used_frames);
    return 0;
}

u32 pmem_alloc_region(size_t num_frames, u32 min_addr, u32 max_addr) {
    if (num_frames == 0 || min_addr >= max_addr) {
        return 0;
    }

    u32 start_frame = (min_addr + FRAME_SIZE - 1) / FRAME_SIZE;
    u32 end_frame = max_addr / FRAME_SIZE;
    if (end_frame > BITMAP_SIZE * 8) {
        end_frame = BITMAP_SIZE * 8;
    }
    if (start_frame >= end_frame) {
        return 0;
    }

    for (u32 frame = start_frame; frame + num_frames <= end_frame; frame++) {
        if (!get_frame(frame)) {
            u8 found = 1;
            for (u32 i = 1; i < num_frames; i++) {
                if (get_frame(frame + i)) {
                    found = 0;
                    break;
                }
            }
            if (found) {
                u32 addr = frame * FRAME_SIZE;
                for (u32 i = 0; i < num_frames; i++) {
                    ref_frame(frame + i);
                }
                kprintf("pmem_alloc_region: Allocated %u frame(s) at addr=%x (frame %u) in range %x-%x\n",
                        num_frames, addr, frame, min_addr, max_addr);
                return addr;
            }
        }
    }

    return 0;
}

void pmem_free(u32 addr, size_t num_frames) {
    u32 frame = addr / FRAME_SIZE;
    for (u32 i = 0; i < num_frames; i++) {
        deref_frame(frame + i);
    }
}

void pmem_claim(u32 addr, size_t num_frames) {
    u32 frame = addr / FRAME_SIZE;
    for (u32 i = 0; i < num_frames; i++) {
        ref_frame(frame + i);
    }
}

void pmem_refcount_inc(u32 addr) {
    u32 frame = addr / FRAME_SIZE;
    if (frame < BITMAP_SIZE * 8) {
        ref_frame(frame);
    }
}

void pmem_refcount_dec(u32 addr) {
    u32 frame = addr / FRAME_SIZE;
    if (frame < BITMAP_SIZE * 8) {
        deref_frame(frame);
    }
}

u32 pmem_get_refcount(u32 addr) {
    u32 frame = addr / FRAME_SIZE;
    if (frame < BITMAP_SIZE * 8) {
        return frame_refcount[frame];
    }
    return 0;
}

u32 pmem_get_total(void) {
    return total_frames * FRAME_SIZE;
}

u32 pmem_get_used(void) {
    return used_frames * FRAME_SIZE;
}

u32 pmem_get_free(void) {
    return (total_frames - used_frames) * FRAME_SIZE;
}