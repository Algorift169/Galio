#include "uname.h"
#include "kprintf.h"
#include "string.h"
#include "user_syscall.h"

u8 shell_uname_command(const char *args, const char *current_dir) {
    (void)current_dir;
    struct utsname info;
    if (args && *args != '\0') {
        if (strcmp(args, "-a") == 0 || strcmp(args, "--all") == 0) {
            if (sys_uname(&info) == 0) {
                kprintf("Galio %s %s %s %s %s\n",
                        info.sysname, info.nodename, info.release,
                        info.version, info.machine);
                return 1;
            }
            kprintf("uname: syscall failed\n");
            return 0;
        }
        if (strcmp(args, "-s") == 0) {
            if (sys_uname(&info) == 0) {
                kprintf("%s\n", info.sysname);
                return 1;
            }
        }
    }

    if (sys_uname(&info) == 0) {
        kprintf("%s %s %s %s %s\n",
                info.sysname, info.nodename, info.release,
                info.version, info.machine);
        return 1;
    }

    kprintf("uname: syscall failed\n");
    return 0;
}
