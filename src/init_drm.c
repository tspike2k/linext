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
The code for initializing and using DRM is based primarily on an old BetterOS article and
the YouTube tutorial series "OS development using the Linux kernel" by Low Level Devel.
The last two videos in particular covered DRM/KMS served as the foundation for most of this
code.

Here is an archive of the BetterOS article:
https://web.archive.org/web/20170607214158/http://betteros.org/tut/graphics1.php

Here are the two video and source code for the Low Level Devel series:
https://www.youtube.com/watch?v=86tz5m0hy9M
https://www.youtube.com/watch?v=wjnLBjLM2QQ
https://github.com/rockytriton/LLD/tree/main/linux_os
*/


#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <stdlib.h> // malloc,free
#include <poll.h>

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

#define ns_from_sec(seconds) (u64)(((f32)(seconds))*1000000000.0f)

#define free_if_nonzero(a) if((a)) free((a))
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

typedef struct{
    u32 *data;
    u32  width;
    u32  height;
    u32  stride;
    u32  handle;
    u32  fb_id;
    u32  total_bytes;
} DRM_Framebuffer;

static void draw_gradiant(DRM_Framebuffer *dest, f32 target_x, f32 target_y){
#if 0
    u32 line_advance = dest->width - dest->stride;
    u32 *pixels = dest->data;
    for_count(u32, y, dest->height){
        for_count(u32, x, dest->width){
            f32 dx = ((f32)x) / (f32)dest->width;
            f32 dy = ((f32)y) / (f32)dest->height;

            f32 value = 1.0f-dist(dx - target_x, dy - target_y);
            u32 b = (u8)(value * 255.0f);

            u32 color = 0xff000000 | (b << 0);
            *pixels = color;
            pixels++;
        }
        pixels += line_advance;
    }
#else
    for_count(u32, y, dest->height){
        for_count(u32, x, dest->width){
            f32 dx = ((f32)x) / (f32)dest->width;
            f32 dy = ((f32)y) / (f32)dest->height;

            f32 value = 1.0f-dist(dx - target_x, dy - target_y);
            u32 b = (u8)(value * 255.0f);

            u32 color = 0xff000000 | (b << 0);
            dest->data[x + y * dest->stride] = color;
        }
    }
#endif
}

// Double buffering
#define Framebuffers_Count 2

typedef struct {
    int fd;
    struct drm_mode_card_res card_resources;
    DRM_Framebuffer framebuffers[Framebuffers_Count];
    u32 crtc_id;
} DRM_State;

static u64 maybe_alloc_drm_ptr(u32 count, u32 size){
    u64 result = 0;
    if(count){
        result = (u64)calloc(1, count*size);
    }
    return result;
}

#define IOCTL_ERROR -1

// DRM mode states from here:
// https://www.youtube.com/watch?v=86tz5m0hy9M
enum{
    DRM_Mode_Connected    = 1,
    DRM_Mode_Disconnected = 2,
    DRM_Mode_Unknown      = 3,
};

static void drm_free_connector(struct drm_mode_get_connector *connector){
    if(connector->encoders_ptr)    free((void *)connector->encoders_ptr);
    if(connector->modes_ptr)       free((void *)connector->modes_ptr);
    if(connector->props_ptr)       free((void *)connector->props_ptr);
    if(connector->prop_values_ptr) free((void *)connector->prop_values_ptr);
}

static u64 get_timestamp_ns(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    u64 result = ts.tv_sec * 1000000000 + ts.tv_nsec;
    return result;
}

// Adapted from the following sources:
// https://web.archive.org/web/20170607214158/http://betteros.org/tut/graphics1.php#doublebuffer
static bool drm_init(DRM_State *s){
    const char *device_path = "/dev/dri/card0";

    s->fd = open(device_path, O_RDWR);
    if(s->fd == -1){
        printf("Failed to open %s: %s\n", device_path, strerror(errno));
        return false;
    }

    // Check to make sure the drm device support dumb buffers.
    // Thanks to "OS development using the Linux kernel - DRM/KMS Video Modes (Part 9)"
    // by Low Level Devel (https://www.youtube.com/watch?v=86tz5m0hy9M)
    u64 drm_caps[2] = {DRM_CAP_DUMB_BUFFER, 0};
    if(ioctl(s->fd, DRM_IOCTL_GET_CAP, drm_caps) == IOCTL_ERROR){
        printf("Unable to query capabilities for device %s: %s.\n", device_path, strerror(errno));
        return false;
    }

    if(!drm_caps[1]){
        printf("Device %s does not support dumb buffers.\n", device_path);
        return false;
    }

    // Become the master of the DRI device, needed to configure Kernel Mode Setting (KMS).
    if(ioctl(s->fd, DRM_IOCTL_SET_MASTER, 0) == IOCTL_ERROR){
        printf("Unable to become master of %s: %s\n", device_path, strerror(errno));
        return false;
    }

    // Much like Vulkan, the API expects you to request card resources once to get the count
    // of each resource and then again after allocating to get the actual resources. To
    // retrieve the inital count, the memory MUST be initialized to zero. We already did
    // this before the call, but it's important to remember.
    /*memset(&s->card_resources, 0, sizeof(s->card_resources);*/
    if(ioctl(s->fd, DRM_IOCTL_MODE_GETRESOURCES, &s->card_resources) == IOCTL_ERROR){
        printf("Unable to get resource count for %s: %s\n", device_path, strerror(errno));
        return false;
    }

    struct drm_mode_card_res *cr = &s->card_resources;
    printf("%u connectors found for device %s\n", cr->count_connectors, device_path);
    if(cr->count_connectors < 1){
        printf("No connectors found for device %s\n", device_path);
        return false;
    }

    // Allocate space for each resource ID.
    // TODO: Use something other than malloc?
    cr->fb_id_ptr        = maybe_alloc_drm_ptr(cr->count_fbs, sizeof(u64));
    cr->crtc_id_ptr      = maybe_alloc_drm_ptr(cr->count_crtcs, sizeof(u64));
    cr->connector_id_ptr = maybe_alloc_drm_ptr(cr->count_connectors, sizeof(u64));
    cr->encoder_id_ptr   = maybe_alloc_drm_ptr(cr->count_encoders, sizeof(u64));

    // Request the ids for each card resource now that we have allocated space for them.
    if(ioctl(s->fd, DRM_IOCTL_MODE_GETRESOURCES, cr) == IOCTL_ERROR){
        printf("Unable to get DRM resource IDs for %s: %s\n", device_path, strerror(errno));
        return false;
    }

    // Loop through each physical connection on the graphics card.
    bool dumb_buffer_success = false;
    for_count(u32, connector_index, cr->count_connectors){
        u64 *connector_ids = (u64 *)cr->connector_id_ptr;
        u64 connector_id = connector_ids[connector_index];

        printf("Testing connector %u (id %u)...\n", connector_index, connector_id);

        // Get connection information, including resource counts and encoder id.
        struct drm_mode_get_connector connector = {0};
        connector.connector_id = connector_id;
        if(ioctl(s->fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector) == IOCTL_ERROR){
            printf("Unable to get connector resources count: %s. Skipping.\n", strerror(errno));
            continue;
        }

        // If the connection is not currently connected, skip it.
        if(connector.connection != DRM_Mode_Connected){
            printf("Connector is not connected. Skipping.\n");
            continue;
        }

        if(!connector.encoder_id){
            printf("Invalid encoder id. Skipping.\n");
            continue;
        }

        connector.encoders_ptr    = maybe_alloc_drm_ptr(connector.count_encoders, sizeof(u64));
        connector.modes_ptr       = maybe_alloc_drm_ptr(connector.count_modes, sizeof(struct drm_mode_modeinfo));
        connector.props_ptr       = maybe_alloc_drm_ptr(connector.count_props, sizeof(u64));
        connector.prop_values_ptr = maybe_alloc_drm_ptr(connector.count_props, sizeof(u64));

        // Get the resources for the connection
        if(ioctl(s->fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector) == IOCTL_ERROR){
            // TODO: Log information about the given connector
            printf("Unable to get connector resources: %s. Skipping.\n", strerror(errno));
            drm_free_connector(&connector);
            continue;
        }

        // Look for the best mode supported by the given device.
        struct drm_mode_modeinfo *modes = (struct drm_mode_modeinfo *)connector.modes_ptr;
        struct drm_mode_modeinfo *best_mode = NULL;
        u32 best_mode_score = 0;
        for_count(u32, mode_index, connector.count_modes){
            struct drm_mode_modeinfo *mode = &modes[mode_index];

            printf("mode %03d: %ux%u\n", mode_index, mode->hdisplay, mode->vdisplay);

            // TODO: Do modes include bit-depth? Can we get this information? We don't want to
            // pick a mode based on resolution and then get bitten down the line when the
            // bit-depth is too low.
            u32 mode_score = mode->hdisplay * mode->vdisplay;
            if(mode_score > best_mode_score){
                best_mode_score = mode_score;
                best_mode = mode;
            }

            // TODO: This is Qemu test code!
            // TODO: Make a Qemu-specific build for this?
            if(mode->hdisplay == 800 && mode->vdisplay == 600){
                best_mode = mode;
                break;
            }
        }

        if(best_mode){
            printf("Best display mode: %ux%u.\n", best_mode->hdisplay, best_mode->vdisplay);
        }
        else{
            printf("Unable to find best mode. Skipping.\n");
            drm_free_connector(&connector);
            continue;
        }

        //
        // Create each framebuffer.
        //
        bool framebuffer_error = false;
        u32 target_framebuffer_width  = best_mode->hdisplay;
        u32 target_framebuffer_height = best_mode->vdisplay;
        for_count(u32, framebuffer_index, Framebuffers_Count){
            // Try to create a dumb buffer
            struct drm_mode_create_dumb create_dumb = {0};
            create_dumb.width  = target_framebuffer_width;
            create_dumb.height = target_framebuffer_height;
            create_dumb.bpp    = 32;
            if(ioctl(s->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb) == IOCTL_ERROR){
                printf("Unable to create dumb buffer: %s. Skipping.\n", strerror(errno));
                framebuffer_error = true;
                break;
            }

            // Try to add the dumb buffer
            struct drm_mode_fb_cmd fb_dumb = {0};
            fb_dumb.handle = create_dumb.handle;
            fb_dumb.width  = create_dumb.width;
            fb_dumb.height = create_dumb.height;
            fb_dumb.bpp    = create_dumb.bpp;
            fb_dumb.pitch  = create_dumb.pitch;
            fb_dumb.depth  = 24;
            if(ioctl(s->fd, DRM_IOCTL_MODE_ADDFB, &fb_dumb) == IOCTL_ERROR){
                printf("Unable to add framebuffer: %s. Skipping.\n", strerror(errno));
                framebuffer_error = true;
                break;
            }

            // Try to map the dumb buffer.
            struct drm_mode_map_dumb map_dumb = {0};
            map_dumb.handle = create_dumb.handle;
            if(ioctl(s->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb) == IOCTL_ERROR){
                printf("Unable to map dumb buffer: %s. Skipping.\n", strerror(errno));
                framebuffer_error = true;
                break;
            }

            DRM_Framebuffer *framebuffer = &s->framebuffers[framebuffer_index];
            framebuffer->width       = create_dumb.width;
            framebuffer->height      = create_dumb.height;
            framebuffer->stride      = create_dumb.pitch / (create_dumb.bpp / 8);
            framebuffer->handle      = create_dumb.handle;
            framebuffer->fb_id       = fb_dumb.fb_id;
            framebuffer->total_bytes = create_dumb.size;

            framebuffer->data   = (u32 *)mmap(0, framebuffer->total_bytes, PROT_READ|PROT_WRITE, MAP_SHARED, s->fd, map_dumb.offset);
            if(framebuffer->data == MAP_FAILED){
                printf("Unable to mmap shared memory for framebuffer: %s. Skipping.\n", strerror(errno));
                framebuffer_error = true;
                break;
            }


        }

        if(framebuffer_error){
            drm_free_connector(&connector);
            continue;
        }

        //
        // Configure Kernel Mode Setting
        //

        // Get the encoder by the ID supplied by the connection.
		struct drm_mode_get_encoder encoder = {0};
		encoder.encoder_id = connector.encoder_id;
		if(ioctl(s->fd, DRM_IOCTL_MODE_GETENCODER, &encoder) == IOCTL_ERROR){
            printf("Unable to get encoder by ID: %s. Skipping.\n", strerror(errno));
            drm_free_connector(&connector);
            continue;
        }

        // Get the crtc by its ID associated with the given encoder.
		struct drm_mode_crtc crtc = {0};
		crtc.crtc_id = encoder.crtc_id;
		if(ioctl(s->fd, DRM_IOCTL_MODE_GETCRTC, &crtc) == IOCTL_ERROR){
            printf("Unable to get crtc by ID: %s. Skipping.\n", strerror(errno));
            drm_free_connector(&connector);
            continue;
        }

        s->crtc_id = encoder.crtc_id;

        // TODO: This doesn't sit right with me. The framebuffers could potentially have
        // different fb_ids, no? If so, does that mean we need to set MKS for each
        // framebuffer individually? Maybe it's fine since framebuffer[0] is the front
        // buffer?
		crtc.fb_id              = s->framebuffers[0].fb_id;
		crtc.set_connectors_ptr = (u64)&connector_ids[connector_index];
		crtc.count_connectors   = 1;
		crtc.mode               = *best_mode;
		crtc.mode_valid         = 1;
		if(ioctl(s->fd, DRM_IOCTL_MODE_SETCRTC, &crtc) == IOCTL_ERROR){
            printf("Unable to set crtc: %s. Skipping.\n", strerror(errno));
            drm_free_connector(&connector);
            continue;
        }

        dumb_buffer_success = true;
        // TODO: Is it safe to free the memory here? We passed DRM some of these pointers
        // via ioctls. Does DRM need to reference that memory after this point? I have no idea.
        // Maybe it would be safer to keep it allocated and then only free at the call to
        // drm_destroy?
        /*drm_free_connector(&connector);*/
        break;
    }

    if(dumb_buffer_success)
        printf("Successfuly initialized DRM for device %s\n", device_path);

    return dumb_buffer_success;
}

static void drm_destroy(DRM_State *drm){
    // Stop being the master of the DRI device.
    // TODO: Do we really need to test this ioctl?
	ioctl(drm->fd, DRM_IOCTL_DROP_MASTER, 0);

    if(drm->fd != -1){
        close(drm->fd);
        drm->fd = -1;
    }

    // TODO: Low Level Devel restores the original crtc before exiting.
    // Should we do this? It would probably make sense, as it might allow for
    // logging to become visible again to the end-user during shutdown.

    free_if_nonzero((void*)drm->card_resources.fb_id_ptr);
	free_if_nonzero((void*)drm->card_resources.crtc_id_ptr);
	free_if_nonzero((void*)drm->card_resources.connector_id_ptr);
	free_if_nonzero((void*)drm->card_resources.encoder_id_ptr);
}

typedef struct{
    s8 data[3];
} Mouse_Event;

enum{
    Poll_FD_Index_Mouse,
    Poll_FD_Index_DRM,
    Poll_FD_Index_Total,
};

int main(){
    // In order for the kernel to populate /dev with device files, we need to mount /dev as
    // a devtmpfs file system.
    if(mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0){
        printf("Failed to mount devtmpfs: %s\n", strerror(errno));
        return 1;
    }

    // TODO: Mount both /proc and /sys

    u32            poll_fds_count = 0;
    struct pollfd  poll_fds[2];
    struct pollfd *poll_fd_mouse = NULL;
    struct pollfd *poll_fd_drm   = NULL;

    int mouse_fd = open("/dev/input/mice", O_RDONLY|O_NONBLOCK);
    if(mouse_fd == -1){
        printf("Warning! Unable to open /dev/input/mice: %s\n", strerror(errno));
    }
    else{
        poll_fd_mouse = &poll_fds[poll_fds_count++];
        poll_fd_mouse->fd = mouse_fd;
        poll_fd_mouse->events = POLLIN;
    }

    DRM_State drm = {0};
    if(drm_init(&drm)){
        int mouse_x = 0;
        int mouse_y = 0;

        u64 target_frame_time_ns = ns_from_sec(1.0f/60.0f);
        u64 prev_timestamp_ns = get_timestamp_ns();

        draw_gradiant(&drm.framebuffers[0], 0.5f, 0.5f);
        u32 backbuffer_index = 1;

        poll_fd_drm = &poll_fds[poll_fds_count++];
        poll_fd_drm->fd     = drm.fd;
        poll_fd_drm->events = POLLIN;

        bool drawn_backbuffer = false;

        while(true){
            if(poll(poll_fds, poll_fds_count, 0) < 0){
                printf("Polling fds failed: %s. Aborting\n", strerror(errno));
                break;
            }

            if(mouse_fd != -1 && poll_fd_mouse->revents & POLLIN){
                printf("Before mouse read\n");
                char buffer[1024];
                // TODO: Handle interrupts
                ssize_t bytes_read = read(mouse_fd, buffer, sizeof(buffer));
                for(u32 i = 0; i < bytes_read; i += sizeof(3)){
                    Mouse_Event *evt = (Mouse_Event *)&buffer[i];
                    mouse_x += evt->data[1];
                    mouse_y += evt->data[2];
                }
                printf("After mouse read\n");
            }

            if(!drawn_backbuffer){
                DRM_Framebuffer *dest = &drm.framebuffers[backbuffer_index];
                f32 target_x = 0.5f + ((f32)mouse_x) / (f32)dest->width;
                f32 target_y = 0.5f - ((f32)mouse_y) / (f32)dest->height;
                draw_gradiant(dest, target_x, target_y);

                // Request a page flip event for when DRM has finished displaying the frontbuffer.
                struct drm_mode_crtc_page_flip flip_req = {0};
                flip_req.fb_id = dest->fb_id;
                flip_req.crtc_id = drm.crtc_id;
                flip_req.flags = DRM_MODE_PAGE_FLIP_EVENT;

                if(ioctl(drm.fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip_req) == IOCTL_ERROR){
                    printf("Unable to request page flip: %s\n", strerror(errno));
                    break;
                }

                drawn_backbuffer = true;
            }

            /*if(poll_fd_drm->revents & POLLIN)*/
            // TODO: Here we're doing a blocking read so that we can wait until the page can be
            // flipped. This is pretty hacky, but it does mean we're not burning CPU time
            // in a busy loop waiting for this to occur. There is probably a much smarter way
            // to handle this.
            {
                char drm_buffer[1024];
                // TODO: Handle interrupts
                ssize_t bytes_read = read(drm.fd, drm_buffer, sizeof(drm_buffer));
                for(u32 i = 0; i < bytes_read;){
                    struct drm_event *evt = (struct drm_event *)&drm_buffer[i];

                    switch(evt->type){
                        case DRM_EVENT_FLIP_COMPLETE:{
                            backbuffer_index = (backbuffer_index + 1) % Framebuffers_Count;
                            drawn_backbuffer = false;
                        } break;
                    }

                    i += evt->length;
                }
            /*}*/
#if 0

            // Frame-rate limiter
            u64 current_timestamp_ns = get_timestamp_ns();
            u64 elasped_time_ns = current_timestamp_ns - prev_timestamp_ns;
            if(elasped_time_ns < target_frame_time_ns){
                sleep_ns(target_frame_time_ns - elasped_time_ns);
            }

            prev_timestamp_ns = current_timestamp_ns;
#endif
        }
    }
    drm_destroy(&drm);

    // TODO: How do we gracefully exit when we're the init script/program?
    while(1){

    }
}
