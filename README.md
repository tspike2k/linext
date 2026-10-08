# Linext

Alternative Linux userland.

## About

This project is an attempt at creating an alternative to the GNU/Linux userland. Though an incredibly important work by a veritable army of developers across the world, GNU/Linux has several flaws that serve to complicate the experience of both end-users and developers. A modern replacement could be designed to mend some of these issues, pushing Linux towards a future where its easier for developers to create and deploy applications and end-users need learn less esoterica to use their computers effectively.

The goal above is perhaps too ambitious for a single developer to tackle as a side project. However, it should prove to be an interesting challenge and an exciting way to learn more about how Linux works at a lower level.

Currently everything presented here is only a proof-of-concept. Bypassing the GNU/Linux userland was trickier than expected. By sharing this project, the author hopes it will be useful for developers with similar aspirations.

## Setup

This will guide you through the process of setting up your developer environment for building and testing this project on Linux. This project is being developed using on a Manjaro distro, but will likely work for Arch or other Arch-based distros. Commands will very likely differ for other Linux distros, however.

It's likely you're distro already comes with these, but these packages are needed to build the project:

`sudo pacman -S gcc tar wget`

For testing via a virtual machine, you will need qemu installed:

`sudo pacman -S qemu-system-x86 qemu-ui-gtk`

## Compiling

To compile, navigate to the root directory of the project. From there, first run the `build_kernel.sh` script to compile the Linux kernel. This may take a while on the first run. Next, run the `build.sh` script to build the userland. At this point the `test.sh` script can be used to run the project inside the qemu virtual machine. Optionally, `build_iso.sh` can be used to create a live image that can either be tested in a virtual machine or burned to a disc/USB stick and run on a physical computer.

## License

This project's source code is shared under the zlib license. See accompanying [license file](https://github.com/tspike2k/linext/blob/main/LICENSE.txt). All dependencies are under their own specific licenses.

## Special Thanks

Thanks to Linus Torvalds and all the Linux kernel developers for making industry strength software that even hobbyists can horse around with in their spare time.
