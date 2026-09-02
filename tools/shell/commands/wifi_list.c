#include "wifi_list.h"
#include "kprintf.h"
#include "net/netdev.h"
#include "net/wifi.h"
#include <string.h>

u8 shell_wifi_list_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (args && *args) {
        kprintf("Usage: wifi-list\n");
        return 0;
    }

    if (!wifi_has_hardware() || !netdev_get_by_name("wlan0")) {
        kprintf("wifi-list: no verified wireless hardware available\n");
        return 0;
    }

    wifi_scan_start();
    u32 count = 0;
    const wifi_scan_result_t *results = wifi_scan_results(&count);
    if (!results || count == 0) {
        kprintf("wifi-list: scan completed with no networks found\n");
        return 1;
    }

    kprintf("Wi-Fi networks (%u):\n", count);
    for (u32 i = 0; i < count; i++) {
        kprintf("  %s  signal=%ddBm  channel=%u\n",
                results[i].ssid, results[i].signal_dbm, results[i].channel);
    }
    return 1;
}
