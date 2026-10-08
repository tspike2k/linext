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

/*
NOTE: In modern day, fbdev is depricated in favor of DRM. Though Linux can be configured to
provide compatibility for fbdev, it is imited and does not seem to support double-buffering.
Though this is a simple example of getting grapical output, these days using fbdev is
not ideal.
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
#include <stdbool.h>

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

static void draw_gradiant(u32 *pixels, u32 pixels_w, u32 pixels_h, u32 stride, f32 target_x, f32 target_y){
    for_count(u32, y, pixels_h){
        for_count(u32, x, pixels_w){
            f32 dx = ((f32)x) / (f32)pixels_w;
            f32 dy = ((f32)y) / (f32)pixels_h;

            f32 value = 1.0f-dist(dx - target_x, dy - target_y);
            u32 b = (u8)(value * 255.0f);

            u32 color = 0xff000000 | (b << 0);
            pixels[x + y * stride] = color;
        }
    }
}

typedef struct{
    int fd;
    u32 *pixels;
    u32  width;
    u32  height;
    u32  stride;
    bool double_buffering;

    struct fb_fix_screeninfo fix_info;
    struct fb_var_screeninfo var_info;
} Fbdev;

static bool init_fbdev(Fbdev *fbdev){
    fbdev->fd = open("/dev/fb0", O_RDWR);
    if(fbdev->fd == -1){
        printf("Failed to open fb0: %s\n", strerror(errno));
        return false;
    }

    //Get fixed screen information
	if(ioctl(fbdev->fd, FBIOGET_FSCREENINFO, &fbdev->fix_info) == -1){
        printf("Unable to get fbdev fix_info: %s\n", strerror(errno));
        return false;
    }
    size_t prev_mem = fbdev->fix_info.smem_len;

    struct fb_var_screeninfo var_info;

    // First, we get the variable screen info from fbdev. Then we configure what we want,
    // and then submit it. We get the info again to make sure we got back what we requested.
	if(ioctl(fbdev->fd, FBIOGET_VSCREENINFO, &var_info) == -1){
        printf("Unable to get fbdev var_info: %s\n", strerror(errno));
        return false;
    }

    // Set target display parameters.
    var_info.grayscale = 0;
    var_info.bits_per_pixel = 32;

    // Attempt to set virtual resolution to make room for double buffering. Most likely this
    // won't work on modern versions of the Linux kernel, as fbdev is now emulated through
    // DRM and is quite limited in functionality.
    fbdev->var_info = var_info;
    fbdev->var_info.xres_virtual = fbdev->var_info.xres;
    fbdev->var_info.yres_virtual = fbdev->var_info.yres*2;

    if(ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->var_info) == -1){
        printf("Failed to set yres_virtual for /dev/fb0: %s\n", strerror(errno));
        fbdev->double_buffering = false;

        fbdev->var_info = var_info;
        if(ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->var_info) == -1){
            printf("Failed to set desired var_info parameters for /dev/fb0: %s\n", strerror(errno));
            return false;
        }
    }
    else{
        printf("fbdev support double buffering.\n");
        fbdev->double_buffering = true;
    }

    if(ioctl(fbdev->fd, FBIOGET_VSCREENINFO, &fbdev->var_info) == -1){
        printf("Failed to get post-configured var_info from /dev/fb0: %s\n", strerror(errno));
        return false;
    }

    if(fbdev->var_info.grayscale != 0){
        printf("Unable to request color display from fbdev.\n");
        return false;
    }

    if(fbdev->var_info.bits_per_pixel != 32){
        printf("Unable to request 32-bits per pixel from fbdev.\n");
        return false;
    }

    u32 bytes_per_pixel = fbdev->var_info.bits_per_pixel / 8;
    fbdev->width  = fbdev->var_info.xres;
    fbdev->height = fbdev->var_info.yres;

    // According to Google, the line advance can be larger than vinfo.xres due to padding
    // done internally by the hardware. The actual line advance is given by finfo.line_length.
    // It claims this is based on the following source, but it doesn't really line up correctly:
    // https://forums.developer.nvidia.com/t/unhandled-level-3-translation-fault-for-accessing-the-dev-fb0/55447
    fbdev->stride = fbdev->fix_info.line_length / bytes_per_pixel;

    printf("fbdev resolution: %u, %u (stride %u)\n", fbdev->width, fbdev->height, fbdev->stride);

    u32 total_bytes = fbdev->var_info.yres * fbdev->fix_info.line_length;
    if(fbdev->fix_info.smem_len < total_bytes){
        printf("fbdev buffer is less than %u bytes(%u bytes instead).\n", total_bytes, fbdev->fix_info.smem_len);
        return false;
    }

    // Map the device to memory
    fbdev->pixels = (uint32_t *)mmap(0, fbdev->fix_info.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fbdev->fd, 0);
    if(((intptr_t)fbdev->pixels) == -1){
        printf("Failed to mmap fbdev.\n");
        return false;
    }

    return true;
}

#define ns_from_sec(seconds) (u64)(((f32)(seconds))*1000000000.0f)

static u64 get_timestamp_ns(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    u64 result = ts.tv_sec * 1000000000 + ts.tv_nsec;
    return result;
}

#define Pixel_Buffers_Count 2

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

    int mouse_fd = open("/dev/input/mice", O_RDONLY|O_NONBLOCK);
    if(mouse_fd == -1){
        printf("Warning! Unable to open /dev/input/mice: %s\n", strerror(errno));
    }

    Fbdev fbdev = {0};
    if(init_fbdev(&fbdev)){
        int mouse_x = 0;
        int mouse_y = 0;

        s8 mouse_event[3];

        u64 target_frame_time_ns = ns_from_sec(1.0f/60.0f);

        u64 prev_timestamp_ns = get_timestamp_ns();

        u32 *pixel_buffers[Pixel_Buffers_Count];

        pixel_buffers[0] = fbdev.pixels;
        pixel_buffers[1] = fbdev.pixels + fbdev.height;
        u32 pixel_buffer_index = 0;

        while(true){
            if(mouse_fd){
                // TODO: Handle interrupts
                // TODO: Read more than one event at a time.
                ssize_t bytes_read = read(mouse_fd, mouse_event, sizeof(mouse_event));
                if(bytes_read == sizeof(mouse_event)){
                    mouse_x += mouse_event[1];
                    mouse_y += mouse_event[2];
                }
            }

            f32 target_x = 0.5f + ((f32)mouse_x) / (f32)fbdev.width;
            f32 target_y = 0.5f - ((f32)mouse_y) / (f32)fbdev.height;

            u32 *pixels = pixel_buffers[pixel_buffer_index];

            draw_gradiant(pixels, fbdev.width, fbdev.height, fbdev.stride, target_x, target_y);

            if(fbdev.double_buffering){
                ioctl(fbdev.fd, FBIO_WAITFORVSYNC, NULL);

                fbdev.var_info.yoffset = pixel_buffer_index * fbdev.height;
                if(ioctl(fbdev.fd, FBIOPAN_DISPLAY, &fbdev.var_info) == -1){
                    printf("Unable to pan fbdev display.\n");
                }
                pixel_buffer_index = (pixel_buffer_index + 1) % (Pixel_Buffers_Count);
            }
            else{
                u64 current_timestamp_ns = get_timestamp_ns();
                u64 elasped_time_ns = current_timestamp_ns - prev_timestamp_ns;
                if(elasped_time_ns < target_frame_time_ns){
                    sleep_ns(target_frame_time_ns - elasped_time_ns);
                }

                prev_timestamp_ns = current_timestamp_ns;
            }
        }
    }

    if(fbdev.fd != -1){
        close(fbdev.fd);
    }

    // TODO: What is the correct thing to do to shut down the Linux kernel?
    while(true){
        sleep_ns(ns_from_sec(1));
    }

    return 0;
}
