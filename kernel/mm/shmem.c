#include "shmem.h"
#include "heap.h"
#include "paging.h"
#include "pmem.h"
#include "string.h"
#include "syscall/syscall.h"
#include "syscall/shmem.h"

typedef struct {
    u8 used;
    u8 removed;
    u16 attachments;
    u32 key;
    u32 size;
    u32 pages;
    u32 phys[GALIO_SHM_MAX_PAGES];
} shmem_segment_t;

typedef struct {
    u8 used;
    u32 pid;
    i32 shmid;
    uintptr_t address;
} shmem_attachment_t;

static shmem_segment_t segments[GALIO_SHM_MAX_SEGMENTS];
static shmem_attachment_t attachments[GALIO_SHM_MAX_SEGMENTS * 4];

static shmem_segment_t *segment_from_id(i32 shmid) {
    if (shmid < 0 || shmid >= GALIO_SHM_MAX_SEGMENTS || !segments[shmid].used) {
        return NULL;
    }
    return &segments[shmid];
}

static void release_segment(i32 shmid) {
    shmem_segment_t *segment = segment_from_id(shmid);
    if (!segment || segment->attachments != 0) return;
    for (u32 i = 0; i < segment->pages; i++) {
        pmem_free(segment->phys[i], 1);
    }
    memset(segment, 0, sizeof(*segment));
}

static u8 region_overlaps(process_t *proc, uintptr_t start, uintptr_t length) {
    uintptr_t end = start + length;
    if (end < start) return 1;
    for (u32 i = 0; i < proc->mmap_count; i++) {
        mmap_region_t *region = &proc->mmap_regions[i];
        if (!region->length) continue;
        uintptr_t region_end = (uintptr_t)region->start + region->length;
        if (start < region_end && end > region->start) return 1;
    }
    return 0;
}

static uintptr_t find_address(process_t *proc, uintptr_t requested, u32 length) {
    uintptr_t address = requested ? PAGE_ALIGN_DOWN(requested) : PAGE_ALIGN_UP(proc->brk);
    if (address < USER_HEAP_START) address = USER_HEAP_START;
    while (address + length >= address && address + length <= USER_HEAP_END) {
        if (!region_overlaps(proc, address, length)) return address;
        address += PAGE_SIZE;
    }
    return 0;
}

void shmem_init(void) {
    memset(segments, 0, sizeof(segments));
    memset(attachments, 0, sizeof(attachments));
}

i32 shmem_get(u32 key, u32 size, i32 flags) {
    if (size == 0) return -22;
    u32 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages > GALIO_SHM_MAX_PAGES) return -12;

    if (key != 0) {
        for (i32 i = 0; i < GALIO_SHM_MAX_SEGMENTS; i++) {
            if (segments[i].used && !segments[i].removed && segments[i].key == key) {
                if ((flags & GALIO_SHM_EXCL) && (flags & GALIO_SHM_CREAT)) return -17;
                return i;
            }
        }
    }
    if (!(flags & GALIO_SHM_CREAT) && key != 0) return -2;

    i32 shmid = -1;
    for (i32 i = 0; i < GALIO_SHM_MAX_SEGMENTS; i++) {
        if (!segments[i].used) {
            shmid = i;
            break;
        }
    }
    if (shmid < 0) return -28;

    shmem_segment_t *segment = &segments[shmid];
    memset(segment, 0, sizeof(*segment));
    segment->used = 1;
    segment->key = key;
    segment->size = size;
    segment->pages = pages;
    for (u32 i = 0; i < pages; i++) {
        segment->phys[i] = pmem_alloc(1);
        if (!segment->phys[i]) {
            segment->pages = i;
            segment->attachments = 0;
            release_segment(shmid);
            return -12;
        }
    }
    return shmid;
}

uintptr_t shmem_attach(process_t *proc, i32 shmid, uintptr_t requested_addr) {
    shmem_segment_t *segment = segment_from_id(shmid);
    if (!proc || !segment || segment->removed || proc->mmap_count >= PROCESS_MAX_MMAPS) return 0;

    u32 length = segment->pages * PAGE_SIZE;
    uintptr_t address = find_address(proc, requested_addr, length);
    if (!address) return 0;

    u32 attachment_slot = GALIO_SHM_MAX_SEGMENTS * 4;
    for (u32 i = 0; i < GALIO_SHM_MAX_SEGMENTS * 4; i++) {
        if (!attachments[i].used) {
            attachment_slot = i;
            break;
        }
    }
    if (attachment_slot == GALIO_SHM_MAX_SEGMENTS * 4) return 0;

    for (u32 i = 0; i < segment->pages; i++) {
        pmem_refcount_inc(segment->phys[i]);
        paging_map(proc->pagedir, address + i * PAGE_SIZE,
                   segment->phys[i], PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }

    attachments[attachment_slot].used = 1;
    attachments[attachment_slot].pid = proc->pid;
    attachments[attachment_slot].shmid = shmid;
    attachments[attachment_slot].address = address;
    segment->attachments++;
    proc->mmap_regions[proc->mmap_count].start = (u32)address;
    proc->mmap_regions[proc->mmap_count].length = length;
    proc->mmap_regions[proc->mmap_count].fd = (u32)shmid;
    proc->mmap_regions[proc->mmap_count].offset = 0;
    proc->mmap_regions[proc->mmap_count].anonymous = 0;
    proc->mmap_count++;
    return address;
}

i32 shmem_detach(process_t *proc, uintptr_t address) {
    if (!proc || !address) return -22;
    for (u32 i = 0; i < GALIO_SHM_MAX_SEGMENTS * 4; i++) {
        shmem_attachment_t *attachment = &attachments[i];
        if (!attachment->used || attachment->pid != proc->pid || attachment->address != address) continue;
        shmem_segment_t *segment = segment_from_id(attachment->shmid);
        if (!segment) return -22;
        for (u32 page = 0; page < segment->pages; page++) {
            paging_unmap(proc->pagedir, address + page * PAGE_SIZE);
            pmem_refcount_dec(segment->phys[page]);
        }
        attachment->used = 0;
        if (segment->attachments) segment->attachments--;
        if (segment->removed) release_segment(attachment->shmid);
        return 0;
    }
    return -22;
}

void shmem_detach_process(u32 pid) {
    for (u32 i = 0; i < GALIO_SHM_MAX_SEGMENTS * 4; i++) {
        if (attachments[i].used && attachments[i].pid == pid) {
            shmem_segment_t *segment = segment_from_id(attachments[i].shmid);
            if (segment && segment->attachments) segment->attachments--;
            attachments[i].used = 0;
            if (segment && segment->removed) release_segment(attachments[i].shmid);
        }
    }
}

i32 shmem_control(process_t *proc, i32 shmid, i32 command, void *buffer) {
    shmem_segment_t *segment = segment_from_id(shmid);
    (void)proc;
    if (!segment) return -22;
    if (command == GALIO_SHM_RMID) {
        segment->removed = 1;
        if (segment->attachments == 0) release_segment(shmid);
        return 0;
    }
    if (command == GALIO_SHM_STAT) {
        if (!buffer || !validate_user_buffer(buffer, sizeof(galio_shmid_ds_t), 1)) return -14;
        galio_shmid_ds_t snapshot;
        snapshot.key = segment->key;
        snapshot.size = segment->size;
        snapshot.attachments = segment->attachments;
        snapshot.pages = segment->pages;
        memcpy(buffer, &snapshot, sizeof(snapshot));
        return 0;
    }
    return -22;
}