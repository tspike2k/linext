//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2026
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------
#include "draw.h"

static u32 blend_colors_premultiplied_alpha(u32 source, u32 dest){
    // Blending code adapted from Handmade Hero.
    // TODO: Cite which day of Handmade Hero it was from

    // TODO: Gamma corrected colors? See here for more information:
    // https://www.youtube.com/watch?v=fVyzTKCfchw&feature=youtu.be&t=3275
    Vec4 s = {{
        (f32)((source >> 16) & 0xff), (f32)((source >>  8) & 0xff),
        (f32)((source >>  0) & 0xff), (f32)((source >> 24) & 0xff)
    }};

    Vec4 d = {{
        (f32)((dest >> 16) & 0xff), (f32)((dest >>  8) & 0xff),
        (f32)((dest >>  0) & 0xff), (f32)((dest >> 24) & 0xff)
    }};

    float rsa = (s.a / 255.0f);
    float rda = (d.a / 255.0f);
    float inv_rsa = (1.0f - rsa);
    Vec4 out_c = {{
        inv_rsa*d.r + s.r,
        inv_rsa*d.g + s.g,
        inv_rsa*d.b + s.b,
        (rsa + rda  - rsa * rda) * 255.0f
    }};
    u32 result = ((u32)out_c.a) << 24 | ((u32)out_c.r) << 16
                | ((u32)out_c.g) <<  8 | ((u32)out_c.b) <<  0;

    return result;
}

Cbed_API void blit_rect(Blit_Dest *dest, Rect r, f32 z, u32 color){
    Vec2 r_min = rect_min(r);
    Vec2 r_max = rect_max(r);

    int min_x = MAX(r_min.x, 0.0f);
    int min_y = MAX(r_min.y, 0.0f);
    int max_x = MIN(r_max.x, dest->width);
    int max_y = MIN(r_max.y, dest->height);

    for_range(int, y, min_y, max_y){
        for_range(int, x, min_x, max_x){
            dest->pixels[x + y * dest->stride] = color;
        }
    }
}

Cbed_API void blit_sprite(Blit_Dest *dest, Blit_Canvas src, Vec3 top_left){
    int x = (int)top_left.x;
    int y = (int)top_left.y;

    // TODO: This only prevents blitting that tries to draw too far right or down,
    // but now too far left or up. Fix this!
    int width  = MIN(src.width, dest->width - x);
    int height = MIN(src.height, dest->height - y);

    for_count(int, sy, height){
        for_count(int, sx, width){
            u32 *dest_color = &dest->pixels[x + sx + (y + sy) * dest->stride];
            u32 src_color = src.pixels[sx + sy * src.stride];
            u8 alpha = (src_color >> 24) & 0xff;
            // TODO: The blend is likely expensive, but is branching really going to make this
            // any faster? This needs to be investigated.
#if 0
            if(alpha == 0){
                continue;
            }
            else if(alpha == 255){
                *dest_color = src_color;
            }
            else{
                *dest_color = blend_colors_premultiplied_alpha(src_color, *dest_color);
            }
#else
                *dest_color = blend_colors_premultiplied_alpha(src_color, *dest_color);
#endif
        }
    }
}

Cbed_API void blit_text(Blit_Dest *dest, Vec3 baseline, Font *font, String text){
    u8 *font_base = (u8 *)font;
    u32 *font_pixels = (u32 *)&font_base[font->img_pixels_offset];

    Vec2 pen = v2(baseline.x, baseline.y);
    u32 prev_codepoint = '\0';
    for_count(u32, i, text.size){
        char c = text.text[i];

        // When to apply kerning based on sample code from here:
        // https://freetype.org/freetype2/docs/tutorial/step2.html#:~:text=c.%20Kerning
        Font_Glyph *glyph = font_get_glyph(font, c);
        float kerning = font_get_kerning_advance(font, prev_codepoint, c);
        pen.x += kerning;

        if(glyph->width > 0 && glyph->height > 0){
            u32 min_x = glyph->uv_min.x * (f32)font->img_width;
            u32 max_x = glyph->uv_max.x * (f32)font->img_width;
            u32 min_y = glyph->uv_min.y * (f32)font->img_height;
            u32 max_y = glyph->uv_max.y * (f32)font->img_height;

            Blit_Canvas sprite = {
                &font_pixels[min_x + min_y * font->img_width],
                max_x - min_x,
                max_y - min_y,
                font->img_width
            };

            Vec3 p = v3(pen.x + glyph->offset.x, pen.y - sprite.height - glyph->offset.y, baseline.z);
            blit_sprite(dest, sprite, p);
        }

        pen.x += (float)glyph->advance;
        prev_codepoint = c;
    }
}
