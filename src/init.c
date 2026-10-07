/*
Copyright (C) 2026 tspike <www.github.com/tspike2k>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
/*#include <drm/drm.h>*/
/*#include <drm/drm_mode.h>*/
#include <sys/ioctl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <linux/fb.h>

static void print_dir(const char *dir_path){
    DIR *dir = opendir(dir_path);
    while(dir){
        struct dirent *next = readdir(dir);
        if(next){
            printf("%s\n", next->d_name);
        }
        else{
            closedir(dir);
            break;
        }
    }
}

int main(){
    // In order for the kernel to populate /dev with device files, we need to mount /dev as
    // a devtmpfs file system.
    if(mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0){
        printf("Failed to mount devtmpfs: %s\n", strerror(errno));
        return 1;
    }

    /*print_dir("/dev");*/

#if 0
    int dri_fd = open("/dev/dri/card0", O_RDWR);
    if(dri_fd == -1){
        printf("Failed to open DRI fd: %s\n", strerror(errno));
    }
#endif

    int fb_fd = open("/dev/fb0", O_RDWR);
    if(fb_fd == -1){
        printf("Failed to open fb0: %s\n", strerror(errno));
    }

    struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo); //Get fixed screen information

    // First, we get the variable screen info from fbdev. Then we configure what we want,
    // and then submit it. We get the info again to make sure we got back what we requested.
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    vinfo.grayscale = 0;
    vinfo.bits_per_pixel = 32;
    ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    if(vinfo.grayscale != 0){
        printf("Unable to request color display from fbdev.\n");
        return 1;
    }

    if(vinfo.bits_per_pixel != 32){
        printf("Unable to request 32-bits per pixel from fbdev.\n");
        return 1;
    }

    size_t pixels_count = vinfo.xres * vinfo.yres;
    size_t pixels_bytes = pixels_count * vinfo.bits_per_pixel;

    // Map the device to memory
    uint32_t *fb_pixels = (uint32_t *)mmap(0, pixels_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if((intptr_t)fb_pixels == -1){
        printf("Failed to mmap!\n");
        return 1;
    }

    while(true) {
        for(uint32_t i = 0; i < pixels_count; i++) {
            fb_pixels[i] = 0xff0000ff;
        }
        sleep(1);
    }

    // TODO: What is the correct thing to do to shut down the Linux kernel?
    return 0;
}
