#include "framebuffer.h"
#include "kprintf.h"

void framebuffer_test(void) {
    u32 bytes = 0;
    u32 passed = 0;
    u32 failed = 0;

    if (fb_validate_geometry(640, 480, 640 * 4, 32, &bytes) &&
        bytes == 640u * 4u * 480u) passed++; else failed++;
    if (!fb_validate_geometry(0, 480, 640 * 4, 32, NULL)) passed++; else failed++;
    if (!fb_validate_geometry(640, 480, 640 * 4 - 1, 32, NULL)) passed++; else failed++;
    if (!fb_validate_geometry(640, 480, 640 * 4, 24, NULL)) passed++; else failed++;
    if (!fb_validate_geometry(0xFFFFFFFFu, 2, 0xFFFFFFFFu, 32, NULL)) passed++; else failed++;

    kprintf("[KTEST] framebuffer: %u passed, %u failed\n", passed, failed);
}