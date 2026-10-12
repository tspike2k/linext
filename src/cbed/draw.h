//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2026
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------
#ifndef CBED_DRAW_H
#define CBED_DRAW_H

#include "common.h"
#include "font.h"

typedef struct {
    u32 *pixels;
    u32  width;
    u32  height;
    u32  stride;
} Blit_Canvas;

// NOTE: This type is designed to be safely cast to a Blit_Canvas.
typedef struct{
    u32 *pixels;
    u32  width;
    u32  height;
    u32  stride;
    // TODO: Depth-buffer pointer? This could be optional.
    // TODO: Control flags? Things like blending options, enabling depth buffer, etc? This
    // would be easier than passing it to each function.
    u32 tint;
} Blit_Dest;

Cbed_API void blit_rect(Blit_Dest *dest, Rect r, f32 z, u32 color);
Cbed_API void blit_sprite(Blit_Dest *dest, Blit_Canvas src, Vec3 top_left);
Cbed_API void blit_text(Blit_Dest *dest, Vec3 baseline, Font *font, String text);

#endif // CBED_DRAW_H
