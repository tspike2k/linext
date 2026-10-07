#!/bin/bash
# This script will build a bootable ISO image that can be written to a USB drive
# and booted on real hardware. It must be run after build.sh.
#
# Based on the following article
# https://medium.com/@ThyCrow/compiling-the-linux-kernel-and-creating-a-bootable-iso-from-it-6afb8d23ba22

# NOTE: This expects the host system and target system to both be booted through BIOS. UEFI
# will need different commands. See the article listed above.

mkdir -p ./bin/iso/boot/grub

cp ./bin/bzImage ./bin/iso/boot
cp ./bin/initramfs.cpio.gz ./bin/iso/boot
cp ./data/grub_bios_config ./bin/iso/boot/grub/grub.cfg

grub-mkrescue -o ./bin/linext.iso ./bin/iso
