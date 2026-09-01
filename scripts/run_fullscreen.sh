#!/usr/bin/env bash
set -euo pipefail

# run_fullscreen.sh — run galio.iso in QEMU (fullscreen mode)
# Usage:
#   ./run_fullscreen.sh                # run fullscreen with default args
#   ./run_fullscreen.sh --nogui        # run headless (nographic) and print serial to stdout
#   ./run_fullscreen.sh --iso path     # use custom ISO path
#   ./run_fullscreen.sh --qemu-args "...args..."  # pass extra qemu args
#   ./run_fullscreen.sh --fullscreen   # run in fullscreen mode (default)
#
# Examples:
#   ./run_fullscreen.sh
#   ./run_fullscreen.sh --nogui
#   ./run_fullscreen.sh --qemu-args "-m 256M -serial file:serial.log"
#   ./run_fullscreen.sh --fullscreen

ISO="build/bin/galio.iso"
DISK="build/disk.img"
QEMU_BIN="qemu-system-x86_64"
EXTRA_ARGS=""
NOGRAPHIC=false
FULLSCREEN=true

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
        --fullscreen|-f)
            FULLSCREEN=true
            shift
            ;;
        --windowed|-w)
            FULLSCREEN=false
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
    echo "Error: ISO '${ISO}' not found. Build it first (make or ./run_fullscreen.sh --iso <path> if using custom ISO)."
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

# Common QEMU arguments. The disk remains Galio's image; attaching a raw host
# disk here would expose and potentially corrupt the host filesystem.
COMMON_ARGS="${QEMU_ACCEL_ARGS} ${QEMU_CPU_ARGS} -smp 1 -cdrom ${ISO} -drive file=${DISK},format=raw,if=ide,cache=none,index=0,media=disk -m ${GALIO_RAM_MB}M -netdev user,id=net0 -device e1000,netdev=net0"
echo "Using ${QEMU_CPU_ARGS}, ${GALIO_RAM_MB} MB guest RAM, and Galio disk image ${DISK}"

# Run QEMU
if [ "${NOGRAPHIC}" = true ]; then
    # Headless mode: print serial to stdout
    echo "Starting QEMU (headless). Serial output will appear on stdout."
    exec ${QEMU_BIN} ${COMMON_ARGS} -display none -monitor none -serial stdio ${EXTRA_ARGS}
elif [ "${FULLSCREEN}" = true ]; then
    # Fullscreen mode with scaling
    echo "Starting QEMU (fullscreen). Serial logged to serial.log"
    exec ${QEMU_BIN} ${COMMON_ARGS} -vga std -display gtk -full-screen -serial file:serial.log -monitor none -no-reboot ${EXTRA_ARGS}
else
    # Windowed mode with scaling to fill window
    echo "Starting QEMU (windowed). Serial logged to serial.log"
    exec ${QEMU_BIN} ${COMMON_ARGS} -vga std -display gtk -serial file:serial.log -monitor none -no-reboot ${EXTRA_ARGS}
fi
