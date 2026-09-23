#include "drivers/nvme.h"
#include "drivers/block.h"
#include "kernel/dma/dma.h"
#include "pci.h"
#include "paging.h"
#include "cpu.h"
#include "kprintf.h"
#include "string.h"

#define NVME_CLASS 0x01
#define NVME_SUBCLASS 0x08
#define NVME_PROGIF 0x02
#define NVME_CAP 0x0000
#define NVME_VS 0x0008
#define NVME_CC 0x0014
#define NVME_CSTS 0x001C
#define NVME_AQA 0x0024
#define NVME_ASQ 0x0028
#define NVME_ACQ 0x0030
#define NVME_DB 0x1000
#define NVME_CC_EN 0x00000001u
#define NVME_CSTS_RDY 0x00000001u
#define NVME_CSTS_CFS 0x00000002u
#define NVME_ADMIN_IDENTIFY 0x06
#define NVME_ADMIN_CREATE_CQ 0x05
#define NVME_ADMIN_CREATE_SQ 0x01
#define NVME_IO_FLUSH 0x00
#define NVME_IO_WRITE 0x01
#define NVME_IO_READ 0x02
#define NVME_TIMEOUT 1000000u
#define NVME_QUEUE_SIZE 16u
#define NVME_MSIX_VECTOR 63u
#define NVME_COMMAND_SIZE 64u
#define NVME_COMPLETION_SIZE 16u

typedef struct __attribute__((packed)) {
    u8 opcode;
    u8 flags;
    u16 cid;
    u32 nsid;
    u64 reserved;
    u64 metadata;
    u64 prp1;
    u64 prp2;
    u32 cdw10;
    u32 cdw11;
    u32 cdw12;
    u32 cdw13;
    u32 cdw14;
    u32 cdw15;
} nvme_command_t;

typedef struct __attribute__((packed)) {
    u32 result;
    u32 reserved;
    u16 sq_head;
    u16 sq_id;
    u16 cid;
    u16 status;
} nvme_completion_t;

typedef struct {
    pci_device_t *pci;
    volatile u8 *regs;
    u32 doorbell_stride;
    u16 queue_size;
    u16 admin_tail;
    u16 admin_head;
    u8 admin_phase;
    u16 io_tail;
    u16 io_head;
    u8 io_phase;
    u16 next_cid;
    u64 namespace_size;
    u32 sector_size;
    dma_buffer_t admin_sq;
    dma_buffer_t admin_cq;
    dma_buffer_t io_sq;
    dma_buffer_t io_cq;
    dma_buffer_t identify;
    block_device_t block;
    u8 ready;
    u8 msix;
} nvme_controller_t;

static nvme_controller_t nvme;
static u8 nvme_registered;

static inline u32 nvme_read32(u32 offset) {
    return *(volatile u32 *)(nvme.regs + offset);
}

static inline void nvme_write32(u32 offset, u32 value) {
    *(volatile u32 *)(nvme.regs + offset) = value;
}

static inline u64 nvme_read64(u32 offset) {
    return *(volatile u64 *)(nvme.regs + offset);
}

static inline void nvme_write64(u32 offset, u64 value) {
    *(volatile u64 *)(nvme.regs + offset) = value;
}

static int nvme_wait_ready(u8 ready) {
    for (u32 i = 0; i < NVME_TIMEOUT; i++) {
        u32 status = nvme_read32(NVME_CSTS);
        if (status & NVME_CSTS_CFS) return -1;
        if (((status & NVME_CSTS_RDY) != 0u) == (ready != 0u)) return 0;
    }
    return -1;
}

static int nvme_alloc_queue(dma_device_t *device, size_t size, dma_buffer_t *buffer) {
    if (dma_alloc_buffer(device, size, 4096, DMA_BIDIRECTIONAL, buffer) != DMA_OK) return -1;
    if (dma_map(device, buffer, DMA_BIDIRECTIONAL, NULL) != DMA_OK) {
        dma_free_buffer(buffer);
        return -1;
    }
    memset(buffer->vaddr, 0, size);
    return 0;
}

static volatile nvme_command_t *nvme_admin_commands(void) {
    return (volatile nvme_command_t *)nvme.admin_sq.vaddr;
}

static volatile nvme_completion_t *nvme_admin_completions(void) {
    return (volatile nvme_completion_t *)nvme.admin_cq.vaddr;
}

static volatile nvme_command_t *nvme_io_commands(void) {
    return (volatile nvme_command_t *)nvme.io_sq.vaddr;
}

static volatile nvme_completion_t *nvme_io_completions(void) {
    return (volatile nvme_completion_t *)nvme.io_cq.vaddr;
}

static u32 nvme_sq_doorbell(u16 queue) {
    return NVME_DB + (u32)queue * 2u * nvme.doorbell_stride;
}

static u32 nvme_cq_doorbell(u16 queue) {
    return NVME_DB + ((u32)queue * 2u + 1u) * nvme.doorbell_stride;
}

static int nvme_poll_admin(u16 cid, u32 *result) {
    volatile nvme_completion_t *completion = nvme_admin_completions();
    for (u32 i = 0; i < NVME_TIMEOUT; i++) {
        u16 status = completion[nvme.admin_head].status;
        if ((status & 1u) == nvme.admin_phase && completion[nvme.admin_head].cid == cid) {
            if (result) *result = completion[nvme.admin_head].result;
            nvme.admin_head++;
            if (nvme.admin_head == nvme.queue_size) {
                nvme.admin_head = 0;
                nvme.admin_phase ^= 1u;
            }
            nvme_write32(nvme_cq_doorbell(0), nvme.admin_head);
            return (status >> 1) & 0x7Fu ? -1 : 0;
        }
    }
    return -1;
}

static int nvme_submit_admin(nvme_command_t *command, u32 *result) {
    u16 cid = nvme.next_cid++;
    command->cid = cid;
    nvme_admin_commands()[nvme.admin_tail] = *command;
    nvme.admin_tail++;
    if (nvme.admin_tail == nvme.queue_size) nvme.admin_tail = 0;
    nvme_write32(nvme_sq_doorbell(0), nvme.admin_tail);
    return nvme_poll_admin(cid, result);
}

static int nvme_poll_io(u16 cid) {
    volatile nvme_completion_t *completion = nvme_io_completions();
    for (u32 i = 0; i < NVME_TIMEOUT; i++) {
        u16 status = completion[nvme.io_head].status;
        if ((status & 1u) == nvme.io_phase && completion[nvme.io_head].cid == cid) {
            int result = ((status >> 1) & 0x7Fu) ? -1 : 0;
            nvme.io_head++;
            if (nvme.io_head == nvme.queue_size) {
                nvme.io_head = 0;
                nvme.io_phase ^= 1u;
            }
            nvme_write32(nvme_cq_doorbell(1), nvme.io_head);
            return result;
        }
    }
    return -1;
}

static int nvme_submit_io(nvme_command_t *command) {
    u16 cid = nvme.next_cid++;
    command->cid = cid;
    nvme_io_commands()[nvme.io_tail] = *command;
    nvme.io_tail++;
    if (nvme.io_tail == nvme.queue_size) nvme.io_tail = 0;
    nvme_write32(nvme_sq_doorbell(1), nvme.io_tail);
    return nvme_poll_io(cid);
}

static int nvme_create_queues(void) {
    nvme_command_t command;
    memset(&command, 0, sizeof(command));
    command.opcode = NVME_ADMIN_CREATE_CQ;
    command.prp1 = nvme.io_cq.dma_addr;
    command.cdw10 = (nvme.queue_size - 1u) | (1u << 16);
    command.cdw11 = 1u;
    if (nvme_submit_admin(&command, NULL) < 0) return -1;
    memset(&command, 0, sizeof(command));
    command.opcode = NVME_ADMIN_CREATE_SQ;
    command.prp1 = nvme.io_sq.dma_addr;
    command.cdw10 = nvme.queue_size - 1u;
    command.cdw11 = 1u;
    command.cdw12 = 1u;
    return nvme_submit_admin(&command, NULL);
}

static int nvme_identify(void) {
    nvme_command_t command;
    u32 result;
    memset(&command, 0, sizeof(command));
    command.opcode = NVME_ADMIN_IDENTIFY;
    command.prp1 = nvme.identify.dma_addr;
    command.cdw10 = 1u;
    if (nvme_submit_admin(&command, &result) < 0) return -1;
    memset(&command, 0, sizeof(command));
    command.opcode = NVME_ADMIN_IDENTIFY;
    command.nsid = 1u;
    command.prp1 = nvme.identify.dma_addr;
    command.cdw10 = 0u;
    if (nvme_submit_admin(&command, &result) < 0) return -1;
    u8 *data = (u8 *)nvme.identify.vaddr;
    u8 format = data[26] & 0x0Fu;
    u8 shift = data[128u + (u32)format * 4u + 2u];
    nvme.sector_size = 1u << shift;
    nvme.namespace_size = *(u64 *)(data + 0);
    return nvme.sector_size == 512u && nvme.namespace_size != 0u ? 0 : -1;
}

static i32 nvme_block_read(block_device_t *device, u64 lba, u32 count, void *buffer) {
    (void)device;
    if (!nvme.ready || !buffer || count == 0u || count > 8u || lba + count > nvme.namespace_size) return -1;
    nvme_command_t command;
    memset(&command, 0, sizeof(command));
    command.opcode = NVME_IO_READ;
    command.nsid = 1u;
    command.prp1 = (u64)(uintptr_t)buffer;
    command.cdw10 = (u32)lba;
    command.cdw11 = (u32)(lba >> 32);
    command.cdw12 = count - 1u;
    return nvme_submit_io(&command);
}

static i32 nvme_block_write(block_device_t *device, u64 lba, u32 count, const void *buffer) {
    (void)device;
    if (!nvme.ready || !buffer || count == 0u || count > 8u || lba + count > nvme.namespace_size) return -1;
    nvme_command_t command;
    memset(&command, 0, sizeof(command));
    command.opcode = NVME_IO_WRITE;
    command.nsid = 1u;
    command.prp1 = (u64)(uintptr_t)buffer;
    command.cdw10 = (u32)lba;
    command.cdw11 = (u32)(lba >> 32);
    command.cdw12 = count - 1u;
    return nvme_submit_io(&command);
}

static i32 nvme_block_flush(block_device_t *device) {
    (void)device;
    nvme_command_t command;
    memset(&command, 0, sizeof(command));
    command.opcode = NVME_IO_FLUSH;
    command.nsid = 1u;
    return nvme_submit_io(&command);
}

static void nvme_interrupt(registers_t *regs) {
    (void)regs;
}

static int nvme_probe(pci_device_t *device) {
    u64 cap;
    u32 cc;
    u32 version;
    if (nvme.ready || !device || device->class_id != NVME_CLASS ||
        device->subclass != NVME_SUBCLASS || device->prog_if != NVME_PROGIF ||
        !device->bars[0] || !device->bar_is_mem[0]) return -1;
    kprintf("NVMe: probing %02x:%02x.%u BAR0=%llx\n", device->bus,
            device->device, device->function,
            (unsigned long long)device->bars[0]);
    memset(&nvme, 0, sizeof(nvme));
    nvme.pci = device;
    nvme.regs = (volatile u8 *)mmio_map_physical(device->bars[0], 0x4000);
    if (!nvme.regs) return -1;
    cap = nvme_read64(NVME_CAP);
    version = nvme_read32(NVME_VS);
    kprintf("NVMe: CAP=%llx VS=%x\n", (unsigned long long)cap, version);
    nvme.doorbell_stride = 4u << ((cap >> 32) & 0xFu);
    nvme.queue_size = (u16)((cap & 0xFFFFu) + 1u);
    if (nvme.queue_size > NVME_QUEUE_SIZE) nvme.queue_size = NVME_QUEUE_SIZE;
    if (nvme.queue_size < 2u || version < 0x00010000u) {
        kprintf("NVMe: unsupported CAP/VS cap=%llx vs=%x\n",
                (unsigned long long)cap, version);
        return -1;
    }
    cc = nvme_read32(NVME_CC) & ~NVME_CC_EN;
    nvme_write32(NVME_CC, cc);
    if (nvme_wait_ready(0) < 0) {
        kprintf("NVMe: controller did not disable\n");
        return -1;
    }
    if (nvme_alloc_queue(&device->dma, nvme.queue_size * NVME_COMMAND_SIZE, &nvme.admin_sq) < 0 ||
        nvme_alloc_queue(&device->dma, nvme.queue_size * NVME_COMPLETION_SIZE, &nvme.admin_cq) < 0 ||
        nvme_alloc_queue(&device->dma, nvme.queue_size * NVME_COMMAND_SIZE, &nvme.io_sq) < 0 ||
        nvme_alloc_queue(&device->dma, nvme.queue_size * NVME_COMPLETION_SIZE, &nvme.io_cq) < 0 ||
        nvme_alloc_queue(&device->dma, 4096, &nvme.identify) < 0) {
        kprintf("NVMe: queue DMA allocation failed\n");
        return -1;
    }
    nvme.admin_phase = 1u;
    nvme.io_phase = 1u;
    nvme_write32(NVME_AQA, (nvme.queue_size - 1u) | ((u32)(nvme.queue_size - 1u) << 16));
    nvme_write64(NVME_ASQ, nvme.admin_sq.dma_addr);
    nvme_write64(NVME_ACQ, nvme.admin_cq.dma_addr);
    nvme_write32(NVME_CC, (6u << 16) | (4u << 20) | NVME_CC_EN);
    if (nvme_wait_ready(1) < 0) {
        kprintf("NVMe: controller did not enable\n");
        return -1;
    }
    if (nvme_identify() < 0) {
        kprintf("NVMe: identify failed\n");
        return -1;
    }
    if (nvme_create_queues() < 0) {
        kprintf("NVMe: I/O queue creation failed\n");
        return -1;
    }
    interrupt_install_handler(NVME_MSIX_VECTOR, nvme_interrupt);
    if (pci_enable_msix(device, NVME_MSIX_VECTOR) == 0) nvme.msix = 1;
    nvme.block.name = "nvme";
    nvme.block.sector_size = nvme.sector_size;
    nvme.block.sector_count = nvme.namespace_size;
    nvme.block.read = nvme_block_read;
    nvme.block.write = nvme_block_write;
    nvme.block.flush = nvme_block_flush;
    nvme.block.private_data = &nvme;
    nvme.ready = 1;
    block_device_register(&nvme.block);
    kprintf("NVMe: controller ready, namespace sectors=%llu%s\n",
            (unsigned long long)nvme.namespace_size, nvme.msix ? " MSI-X" : " polled");
    return 0;
}

static pci_driver_t nvme_driver = {
    .vendor_id = 0xFFFF,
    .device_id = 0xFFFF,
    .probe = nvme_probe
};

void nvme_register_driver(void) {
    if (!nvme_registered) {
        nvme_registered = 1;
        pci_register_driver(&nvme_driver);
    }
}

int nvme_is_available(void) {
    return nvme.ready != 0;
}
