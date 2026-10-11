#!/bin/bash
# Build the userland for testing in a virtual machine.
FLAGS="-static -g -DVM_Target"

mkdir -p ./bin/rootfs
gcc $FLAGS -o ./bin/rootfs/init src/init_drm.c -I/usr/include/drm -lm

# Build the initramfs file so Linux can boot our init code.
mkdir -p ./bin/rootfs/dev
mkdir -p ./bin/rootfs/proc
mkdir -p ./bin/rootfs/sys

# Gather all the files and directories listed in ./bin/rootfs and then convert them into a
# cpio archive, which we compress using gzip.
cd ./bin/rootfs
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
