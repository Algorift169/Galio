#!/usr/bin/env bash
set -euo pipefail

# simp_run.sh — simple QEMU runner for quick testing
# Defaults: boot build/bin/galio.iso with a small disk image and NO network device
# Use `--net` to enable the default user-mode e1000 network device.

ISO="build/bin/galio.iso"
DISK="build/disk.img"
QEMU_BIN="qemu-system-x86_64"
EXTRA_ARGS=""
NOGRAPHIC=false
FULLSCREEN=false

# Prefer GUI when a display is available; otherwise fall back to headless serial mode.
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    NOGRAPHIC=true
fi

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
        --fullscreen|-f)
            FULLSCREEN=true
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

command -v "${QEMU_BIN}" >/dev/null 2>&1 || {
    echo "Error: ${QEMU_BIN} not found in PATH"
    exit 1
}

if [ ! -f "${ISO}" ]; then
    echo "Error: ISO '${ISO}' not found. Build it first (make)."
    exit 1
fi

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
# can use the interface.
COMMON_ARGS="-cdrom ${ISO} -drive file=${DISK},format=raw,if=ide,cache=none,index=0,media=disk -m 128M -netdev user,id=net0,restrict=off -device e1000,netdev=net0"

echo "Using QEMU user-mode NAT networking through e1000"

if [ "${NOGRAPHIC}" = true ]; then
    echo "Starting QEMU (headless). Serial output will appear on stdout."
    exec ${QEMU_BIN} ${COMMON_ARGS} -display none -monitor none -serial stdio ${EXTRA_ARGS}
elif [ "${FULLSCREEN}" = true ]; then
    echo "Starting QEMU (fullscreen). Serial logged to serial.log"
    exec ${QEMU_BIN} ${COMMON_ARGS} -vga std -full-screen -display gtk,zoom-to-fit=on -serial file:serial.log -monitor none -no-reboot ${EXTRA_ARGS}
else
    echo "Starting QEMU (windowed). Serial logged to serial.log"
    exec ${QEMU_BIN} ${COMMON_ARGS} -vga std -display gtk,zoom-to-fit=on -serial file:serial.log -monitor none -no-reboot ${EXTRA_ARGS}
fi
