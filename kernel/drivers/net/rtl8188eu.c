/* rtl8188eu.c - RTL8188EU binding status for Galio. */
#include "rtl8188eu.h"
#include "lib/kprintf.h"

void rtl8188eu_register_driver(void) {
    /* USB enumeration, firmware loading, and data transfers are incomplete. */
    kprintf("rtl8188eu: unavailable; verified USB binding is not implemented\n");
}
