#!/bin/bash
KERNEL_NAME="linux-5.10.270"

mkdir -p ./bin/
# Download the version of the Linux kernel we want to use. The -nc option should avoid downloading
# the file if it already exists at the target destination. The -P option specifies the destination
# directory path.
#
# Sources:
# https://stackoverflow.com/questions/4944295/skip-download-if-files-already-exist-in-wget
wget -nc -P ./data https://cdn.kernel.org/pub/linux/kernel/v5.x/$KERNEL_NAME.tar.xz
echo Extracting...
cd bin
tar -x --skip-old-files -f ../data/$KERNEL_NAME.tar.xz
cd $KERNEL_NAME
# NOTE: You can use "make menuconfig" for a "graphical" editor to configure the kernel build
# options.
#echo Configuring...
#make defconfig
cp ../../data/kernel_config .config

echo Building...
make -j 10
cp ./arch/x86/boot/bzImage ../
