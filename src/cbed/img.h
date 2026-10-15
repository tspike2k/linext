//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2025
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

#ifndef CBED_IMG_H
#define CBED_IMG_H

#include "common.h"

// Internally, colors are in the 32-bit RGBA LE format. This means hex literals for colors
// have the following format: 0xAABBGGRR
Cbed_API u32 premultiply_alpha(u32 c);
Cbed_API u32 bgra_to_rgba(u32 c);
Cbed_API u32 rgba_to_bgra(u32 c);
Cbed_API u32 color_from_v4(Vec4 v);
Cbed_API Vec4 v4_from_color(u32 c);

typedef struct{
    u32 *data;
    u32 width;
    u32 height;
} Img_Pixels;

Cbed_API Img_Pixels img_load_tga_from_memory(const char *file_name, void* data, size_t data_size, Buffer *dest);
Cbed_API Img_Pixels img_load_tga(const char* file_name, Buffer* dest, Buffer *scratch);
Cbed_API bool img_save_tga(const char* file_name, u32 width, u32 height, u32 *pixels, Buffer *scratch);

#endif // CBED_IMG_H
