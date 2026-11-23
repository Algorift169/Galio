#!/usr/bin/env bash
set -euo pipefail

# run.sh — run galio.iso in QEMU
# Usage:
#   ./run.sh                # run with default args (GUI, serial -> serial.log)
#   ./run.sh --nogui        # run headless (nographic) and print serial to stdout
#   ./run.sh --iso path     # use custom ISO path
#   ./run.sh --qemu-args "...args..."  # pass extra qemu args
#
# Examples:
#   ./run.sh
#   ./run.sh --nogui
#   ./run.sh --qemu-args "-m 256M -display gtk,zoom-to-fit=on"

ISO="build/bin/galio.iso"
DISK="build/disk.img"
QEMU_BIN="qemu-system-x86_64"
EXTRA_ARGS=""
NOGRAPHIC=false
GUI_DISPLAY_ARGS="-display gtk,zoom-to-fit=on"
QEMU_DISPLAY_ARGS="-device VGA,edid=on,xres=1280,yres=720"

# Galio's physical-memory bitmap currently supports at most 128 MiB. Keep the
# guest within that limit instead of advertising unmanaged memory to the kernel.
GALIO_RAM_MB="${GALIO_RAM_MB:-128}"
if [[ ! "${GALIO_RAM_MB}" =~ ^[0-9]+$ ]] ||
   (( GALIO_RAM_MB < 96 || GALIO_RAM_MB > 128 )); then
    echo "Error: GALIO_RAM_MB must be between 96 and 128 (kernel limit: 128 MB)"
    exit 1
fi
QEMU_ACCEL_ARGS=""
QEMU_CPU_ARGS="-cpu max"
if [ -e /dev/kvm ] && [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    QEMU_ACCEL_ARGS="-enable-kvm"
    QEMU_CPU_ARGS="-cpu host"
else
    echo "KVM unavailable; using QEMU's maximum CPU model instead of host passthrough"
fi

# Prefer GUI when a display is available; otherwise fall back to headless serial mode.
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    NOGRAPHIC=true
fi

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --nogui|-n)
            NOGRAPHIC=true
            shift
            ;;
        --gui|-g)
            NOGRAPHIC=false
            shift
            ;;
        --iso)
            ISO="$2"
            shift 2
            ;;
        --qemu-args)
            EXTRA_ARGS="$2"
            shift 2
            ;;
        --help|-h)
            sed -n '1,200p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown arg: $1"
            echo "Use --help for usage."
            exit 1
            ;;
    esac
done

# Sanity checks
command -v "${QEMU_BIN}" >/dev/null 2>&1 || {
    echo "Error: ${QEMU_BIN} not found in PATH"
    exit 1
}

if [ ! -f "${ISO}" ]; then
    echo "Error: ISO '${ISO}' not found. Build it first (make or ./run.sh --iso <path> if using custom ISO)."
    exit 1
fi

# Create disk image if it doesn't exist
if [ ! -f "${DISK}" ]; then
    echo "Disk image '${DISK}' not found. Creating 64MB disk image..."
    dd if=/dev/zero of="${DISK}" bs=1M count=64 2>/dev/null || {
        echo "Error: Failed to create disk image"
        exit 1
    }
    if command -v mkfs.ext2 >/dev/null 2>&1; then
        mkfs.ext2 -q "${DISK}" 2>/dev/null || {
            echo "Error: Failed to format disk with ext2"
            exit 1
        }
    elif command -v mke2fs >/dev/null 2>&1; then
        mke2fs -t ext2 -q "${DISK}" 2>/dev/null || {
            echo "Error: Failed to format disk with ext2"
            exit 1
        }
    else
        echo "Error: mkfs.ext2 or mke2fs not found in PATH"
        exit 1
    fi
    echo "Disk image created and formatted."
fi

# QEMU user networking provides outbound NAT through the host's network.
# Its standard gateway, DHCP range, and DNS proxy are pinned for repeatable tests.
# Galio currently starts one CPU; its AP/SMP startup is not enabled yet.
NETDEV_ARGS="-netdev user,id=net0,restrict=off,host=10.0.2.2,dhcpstart=10.0.2.15,dns=10.0.2.3,ipv6=off -device e1000,netdev=net0"
COMMON_ARGS="${QEMU_ACCEL_ARGS} ${QEMU_CPU_ARGS} -smp 1 ${QEMU_DISPLAY_ARGS} -cdrom ${ISO} -drive file=${DISK},format=raw,if=ide,cache=none,index=0,media=disk -m ${GALIO_RAM_MB}M ${NETDEV_ARGS}"
echo "Using ${QEMU_CPU_ARGS}, 1 CPU, ${GALIO_RAM_MB} MB guest RAM, Galio disk image ${DISK}, and e1000 user networking (NAT)"

# Run QEMU
if [ "${NOGRAPHIC}" = true ]; then
    # Headless mode: print serial to stdout without competing with the monitor
    echo "Starting QEMU (headless). Serial output will appear on stdout."
    exec ${QEMU_BIN} ${COMMON_ARGS} -display none -monitor none -serial stdio ${EXTRA_ARGS}
else
    # GUI mode: serial logged to serial.log
    echo "Starting QEMU (windowed, fit-to-window). Serial logged to serial.log"
    exec ${QEMU_BIN} ${COMMON_ARGS} ${GUI_DISPLAY_ARGS} -serial file:serial.log -monitor none -no-reboot ${EXTRA_ARGS}
fi
