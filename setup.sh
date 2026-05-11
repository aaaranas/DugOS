#!/usr/bin/env bash
# DugOS -- WSL2 / Ubuntu toolchain bootstrap.
# Run once on a fresh Ubuntu install:  bash setup.sh
set -e

sudo apt update
sudo apt install -y \
    build-essential \
    gcc-multilib \
    nasm \
    qemu-system-x86 \
    grub-pc-bin \
    grub-common \
    xorriso \
    mtools

echo
echo "DugOS toolchain installed."
echo "Try:  cd src && make run"
