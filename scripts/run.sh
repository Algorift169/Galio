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
#   ./run.sh --qemu-args "-m 256M -display gtk -serial file:serial.log"

ISO="build/bin/galio.iso"
DISK="build/disk.img"
QEMU_BIN="qemu-system-x86_64"
EXTRA_ARGS=""
NOGRAPHIC=false

# Use the host CPU feature set and a safe share of host memory by default.
# Override GALIO_RAM_MB when a different guest size is desired.
HOST_RAM_MB=$(awk '/MemTotal:/ { printf "%d", $2 / 1024 }' /proc/meminfo)
GALIO_RAM_MB="${GALIO_RAM_MB:-$((HOST_RAM_MB / 2))}"
if [ "${GALIO_RAM_MB}" -lt 128 ]; then GALIO_RAM_MB=128; fi
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

# QEMU user networking provides outbound NAT through the host's real network.
# The guest still needs DHCP or static IP configuration before kernel sockets
# can use the interface. The disk remains Galio's image; no host disk is used.
COMMON_ARGS="${QEMU_ACCEL_ARGS} ${QEMU_CPU_ARGS} -smp 1 -cdrom ${ISO} -drive file=${DISK},format=raw,if=ide,cache=none,index=0,media=disk -m ${GALIO_RAM_MB}M -netdev user,id=net0,restrict=off -device e1000,netdev=net0"
echo "Using ${QEMU_CPU_ARGS}, ${GALIO_RAM_MB} MB guest RAM, and Galio disk image ${DISK}"

# Run QEMU
if [ "${NOGRAPHIC}" = true ]; then
    # Headless mode: print serial to stdout without competing with the monitor
    echo "Starting QEMU (headless). Serial output will appear on stdout."
    exec ${QEMU_BIN} ${COMMON_ARGS} -display none -monitor none -serial stdio ${EXTRA_ARGS}
else
    # GUI mode: serial logged to serial.log
    echo "Starting QEMU (GUI). Serial logged to serial.log"
    exec ${QEMU_BIN} ${COMMON_ARGS} -display gtk -serial file:serial.log -monitor none -no-reboot ${EXTRA_ARGS}
fi
