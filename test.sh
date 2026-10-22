#!/bin/bash

#
# NOTE: The "console=ttyS0" parameter routes terminal output to the serial0 device, which
# can be viewed in Qemu-gtk under the View menu or by pressing Ctrl+Alt+4
#

#
# Finding the right configuration options to get Qemu working well has been quite the challenge.
# Most options won't show graphics at all, still more will not play well with Vsynced DRM/KMS
# rendering. It sounds as though you can tell Qemu to use the underlying DRM/KMS device on the
# host system, but this requires hardware that can support it (and probably tricky system
# configuration).
#
# For the longest time, this magical incantation worked quite well:
 qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -device bochs-display -append "console=ttyS0 consoleblank=0 vt.global_cursor_default=0"
#
# Once the guest was rendering in less than 16ms, hideous sprite flickering would occur the likes
# of which would put the NES version of Commando to shame. My best guess is this was caused by
# Qemu not properly handling page flip requests, causing us to swap buffers too quickly and
# write directly into the front buffer.
#
# After some searching and a bit of testing, it seemed as though the fix would be to switch to
# using "-display gtk,gl=on,show-cursor=on". It fixed the flickering, but mouse events stopped
# working altogether. It turns out, to get the same effect you can switch to using SDL
# as the UI target and use that one instead. After that, the mouse worked. Goodness. Gracious.
#
# This is the best working invocation, as far as I can see:
#qemu-system-x86_64 -kernel ./bin/bzImage -initrd ./bin/initramfs.cpio.gz -device bochs-display -display sdl,gl=on -append "console=ttyS0"

# NOTE: Setting the device to "virtio-gpu-gl-pci" also seems to work, but not any better as far
# as I can see. Plus, you need to install more packages for that one.
