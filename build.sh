#!/bin/bash
mkdir -p ./bin/rootfs/dev
gcc -static -o ./bin/rootfs/init src/init.c -I/usr/include/drm

# Gather all the files and directories listed in ./bin/rootfs and then convert them into a
# cpio archive, which we compress using gzip.
cd ./bin/rootfs
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
