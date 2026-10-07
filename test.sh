#!/bin/bash

# These are various failed attemps to get graphics working with Qemu. It's possible they may
# work fine under the right circumstances.
#qemu-system-x86_64 -kernel ./boot/bzImage -initrd ./boot/initramfs.cpio
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -device bochs-display -vga std
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -device bochs-display
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -vga std -device bochs-display
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -vga std -m 512M -append "console=ttyS0 root=/dev/ram0"
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -m 512M -vga std -append "root=/dev/ram0 console=tty0 video=vesafb:ywrap,mode:1024x768-32"
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -m 512M -vga virtio -display gtk,gl=on
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -m 512M -vga virtio
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -append "console=ttyS0 quiet" -vga std


# NOTE: To disable blinking terminal cursor when using fbdev, you need to provide the kernel with
# the following boot parameters to the kernel:
# "consoleblank=0 vt.global_cursor_default=0"
# Sources:
# https://developer.toradex.com/linux-bsp/application-development/multimedia/framebuffer-linux/
#
# NOTE: The "console=ttyS0" parameter routes terminal output to the serial0 device, which
# can be viewed in Qemu-gtk under the View menu or by pressing Ctrl+Alt+4
qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -device bochs-display -append "console=ttyS0 consoleblank=0 vt.global_cursor_default=0"
