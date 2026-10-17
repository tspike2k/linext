#!/bin/bash
# Build the userland for testing in a virtual machine.
FLAGS="-static -O2 -Wall -Wno-unused-variable -Wno-unused-function -DVM_Target"

mkdir -p ./bin/rootfs
gcc $FLAGS -o ./bin/rootfs/init src/init.c -I/usr/include/drm -lm

# Build the initramfs file so Linux can boot our init code.
mkdir -p ./bin/rootfs/dev
mkdir -p ./bin/rootfs/proc
mkdir -p ./bin/rootfs/sys

cp ./data/grass.tga ./bin/rootfs
cp ./data/font_en.fnt ./bin/rootfs

# Gather all the files and directories listed in ./bin/rootfs and then convert them into a
# cpio archive, which we compress using gzip.
cd ./bin/rootfs
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
