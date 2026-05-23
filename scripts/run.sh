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

ISO="build/galio.iso"
DISK="build/disk.img"
QEMU_BIN="qemu-system-i386"
EXTRA_ARGS=""
NOGRAPHIC=false

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --nogui|-n)
            NOGRAPHIC=true
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

# Common QEMU arguments
COMMON_ARGS="-cdrom ${ISO} -drive file=${DISK},format=raw,if=ide,cache=none,index=0,media=disk -m 128M -netdev user,id=net0 -device e1000,netdev=net0"

# Run QEMU
if [ "${NOGRAPHIC}" = true ]; then
    # Headless mode: print serial to stdout
    echo "Starting QEMU (headless). Serial output will appear on stdout."
    exec ${QEMU_BIN} ${COMMON_ARGS} -nographic -serial stdio ${EXTRA_ARGS}
else
    # GUI mode: serial logged to serial.log
    echo "Starting QEMU (GUI). Serial logged to serial.log"
    exec ${QEMU_BIN} ${COMMON_ARGS} -display gtk -serial file:serial.log -monitor none -no-reboot ${EXTRA_ARGS}
fi
