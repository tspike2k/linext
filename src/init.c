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
#include <math.h>
#include <time.h>

typedef int8_t   s8;
typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;

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

#define for_count(T, i, max) for(T i = 0; i < max; i++)

static f32 lerp(f32 a, f32 b, f32 t){
    f32 result = a + (b - a)*t;
    return result;
}

static f32 dist(f32 x, f32 y){
    f32 result = sqrt(x*x + y*y);
    return result;
}

static void sleep_ns(u64 nanoseconds){
    if(nanoseconds > 0){
        struct timespec ts;
        ts.tv_sec  = nanoseconds / 1000000000;
        ts.tv_nsec = nanoseconds % 1000000000;
        // NOTE: nanosleep can fail when a signal is raised. If this happens it returns -1.
        // In that case we try the function again.
        while(nanosleep(&ts, NULL) == -1){

        }
    }
}

static void draw_gradiant(u32 *pixels, u32 pixels_w, u32 pixels_h, f32 target_x, f32 target_y){
    for_count(u32, y, pixels_h){
        for_count(u32, x, pixels_w){
            f32 dx = ((f32)x) / (f32)pixels_w;
            f32 dy = ((f32)y) / (f32)pixels_h;

            f32 value = 1.0f-dist(dx - target_x, dy - target_y);
            u32 b = (u8)(value * 255.0f);

            u32 color = 0xff000000 | (b << 0);
            pixels[x + y * pixels_w] = color;
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

    print_dir("/dev/input");

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

    int mouse_fd = open("/dev/input/mice", O_RDONLY|O_NONBLOCK);
    if(mouse_fd == -1){
        printf("Warning! Unable to open /dev/input/mice: %s\n", strerror(errno));
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

    u32 pixels_w = vinfo.xres;
    u32 pixels_h = vinfo.yres;
    size_t pixels_count =  pixels_w * pixels_h;
    size_t pixels_bytes = pixels_count * vinfo.bits_per_pixel;

    // Map the device to memory
    uint32_t *fb_pixels = (uint32_t *)mmap(0, pixels_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if((intptr_t)fb_pixels == -1){
        printf("Failed to mmap!\n");
        return 1;
    }

    int mouse_x = 0;
    int mouse_y = 0;

    s8 mouse_event[3];

    while(true) {
        if(mouse_fd){
            // TODO: Handle interrupts
            ssize_t bytes_read = read(mouse_fd, mouse_event, sizeof(mouse_event));
            if(bytes_read == sizeof(mouse_event)){
                mouse_x += mouse_event[1];
                mouse_y += mouse_event[2];
            }
        }

        f32 target_x = 0.5f + ((f32)mouse_x) / (f32)pixels_w;
        f32 target_y = 0.5f - ((f32)mouse_y) / (f32)pixels_h;

        draw_gradiant(fb_pixels, pixels_w, pixels_h, target_x, target_y);
        target_x += 0.01f;
        if(target_x > 1) target_x = 0;
        sleep_ns(16000000);
    }

    // TODO: What is the correct thing to do to shut down the Linux kernel?
    return 0;
}
