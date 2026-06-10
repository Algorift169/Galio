#!/usr/bin/env bash
set -euo pipefail

# deps.sh - Install build/runtime dependencies for Galio kernel development
# Supports Debian/Ubuntu (apt), Fedora (dnf), Arch (pacman), and openSUSE (zypper).

REQ_PACKAGES_APT=(build-essential gcc-multilib nasm binutils mtools xorriso grub-pc-bin qemu-system-x86 qemu-utils e2fsprogs pkg-config)
REQ_PACKAGES_DNF=(gcc gcc-c++ glibc-devel.i686 nasm binutils mtools xorriso grub2-tools qemu-system-x86 e2fsprogs)
REQ_PACKAGES_PACMAN=(base-devel nasm binutils mtools xorriso grub qemu e2fsprogs)
REQ_PACKAGES_ZYPPER=(gcc gcc-c++ glibc-devel-32bit nasm binutils mtools xorriso grub2 qemu e2fsprogs)

install_with_apt() {
    sudo apt-get update
    sudo apt-get install -y "${REQ_PACKAGES_APT[@]}"
}

install_with_dnf() {
    sudo dnf install -y "${REQ_PACKAGES_DNF[@]}"
}

install_with_pacman() {
    sudo pacman -Sy --needed --noconfirm "${REQ_PACKAGES_PACMAN[@]}"
}

install_with_zypper() {
    sudo zypper refresh
    sudo zypper install -y "${REQ_PACKAGES_ZYPPER[@]}"
}

detect_and_install() {
    if command -v apt-get >/dev/null 2>&1; then
        echo "Detected apt (Debian/Ubuntu). Installing packages..."
        install_with_apt
        return
    fi

    if command -v dnf >/dev/null 2>&1; then
        echo "Detected dnf (Fedora/RHEL). Installing packages..."
        install_with_dnf
        return
    fi

    if command -v pacman >/dev/null 2>&1; then
        echo "Detected pacman (Arch). Installing packages..."
        install_with_pacman
        return
    fi

    if command -v zypper >/dev/null 2>&1; then
        echo "Detected zypper (openSUSE). Installing packages..."
        install_with_zypper
        return
    fi

    echo "Unsupported distribution: please install the following packages manually:"
    echo "  - build tools: gcc, make, binutils, nasm"
    echo "  - multilib support (gcc-multilib/libc-devel.i686) for -m32 builds"
    echo "  - qemu-system-x86 (or qemu-system-i386), grub-mkrescue, xorriso, mtools"
    echo "  - e2fsprogs (mkfs.ext2), xorriso"
    exit 1
}

echo "This script will install system packages required to build and run the Galio kernel." 
echo "It may prompt for your sudo password."

detect_and_install

echo "Dependency installation complete."
