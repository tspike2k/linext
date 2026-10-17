#!/bin/bash
# This script will build a bootable ISO image that can be written to a USB drive
# and booted on real hardware.
#
# Based on the following article
# https://medium.com/@ThyCrow/compiling-the-linux-kernel-and-creating-a-bootable-iso-from-it-6afb8d23ba22

# NOTE: This expects the host system and target system to both be booted through BIOS. UEFI
# will need different commands. See the article listed above.

FLAGS="-static -O2 -fvisibility=hidden "

mkdir -p ./bin/rootfs
gcc $FLAGS -DCbed_API="static "  -o ./bin/rootfs/init src/init.c -I/usr/include/drm -lm

# Build the initramfs file so Linux can boot our init code.
mkdir -p ./bin/rootfs/dev
mkdir -p ./bin/rootfs/proc
mkdir -p ./bin/rootfs/sys

# Gather all the files and directories listed in ./bin/rootfs and then convert them into a
# cpio archive, which we compress using gzip.
cd ./bin/rootfs
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
cd ../..

# Build the ISO image
mkdir -p ./bin/iso/boot/grub
cp ./bin/bzImage ./bin/iso/boot
cp ./bin/initramfs.cpio.gz ./bin/iso/boot
cp ./data/grub_bios_config ./bin/iso/boot/grub/grub.cfg

grub-mkrescue -o ./bin/linext.iso ./bin/iso
