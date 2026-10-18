# Linext

Alternative Linux userland.

## About

This project is an attempt at creating an alternative to the GNU/Linux userland. Though an incredibly important work by a veritable army of developers across the world, GNU/Linux has several flaws that serve to complicate the experience of both end-users and developers. A modern replacement could be designed to mend some of these issues, pushing Linux towards a future where its easier for developers to create and deploy applications and end-users need learn less esoterica to use their computers effectively.

The goal above is perhaps too ambitious for a single developer to tackle as a side project. However, it should prove to be an interesting challenge and an exciting way to learn more about how Linux works at a lower level.

Currently everything presented here is only a proof-of-concept. Bypassing the GNU/Linux userland was trickier than expected. By sharing this project, the author hopes it will be useful for developers with similar aspirations.

## Setup

This will guide you through the process of setting up your developer environment for building and testing this project on Linux. Development is currently being done on a Manjaro distro, but will likely work for Arch or other Arch-based distros. Commands will very probably differ for other Linux distros, however.

It's likely you're distro already comes with these, but these packages are needed to build the project:

`sudo pacman -S gcc tar wget`

For testing via a virtual machine, you will need Qemu installed:

`sudo pacman -S qemu-system-x86 qemu-ui-sdl`

## Compiling and Testing

To compile, navigate to the root directory of the project. From there, first run the `build_kernel.sh` script to compile the Linux kernel. This may take a while on the first run. After this, you can choose to build for a virtual machine or real hardware. If targeting a virtual machine, run `build.sh`. For real hardware, run `build_iso.sh`.

__WARNING!__ This project is a work-in-progress; though unlikely, running it on real hardware could cause damage to your computer! Use at your own risk.

To test inside a virtual machine, run the `test.sh` script. To test on real hardware, burn the *./bin/linext.iso* live image to a disc or USB stick, insert it into a computer, power the computer on and then select the media as the boot device.

## License

This project's source code is shared under the zlib license. See accompanying [license file](https://github.com/tspike2k/linext/blob/main/LICENSE.txt). Project dependencies are under their own individual licenses.

## Special Thanks

Thanks to Linus Torvalds and all the Linux kernel developers for making industry strength software that even hobbyists can horse around with in their spare time.
