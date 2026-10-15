//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2026
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

// TODO: Better explain how all this works for future reference!

/*
An attempt at using noise to generate something approaching a grass texture for
the Linext project.

Interesting articles about using noise:
https://www.redblobgames.com/articles/noise/introduction.html
https://www.redblobgames.com/maps/terrain-from-noise/#demo
https://web.archive.org/web/20130328191629/http://freespace.virgin.net/hugo.elias/models/m_perlin.htm
*/

#include "cbed/common.c"
#include "cbed/files.c"
#include "cbed/math.c"
#include "cbed/img.c"
#include "cbed/random.c"

static f32 normalized_range_clamp(f32 a, f32 min, f32 max){
    // Handy function thanks to Casey Muratori, Handmade Hero day 107.
    a = CLAMP(a, min, max);
    f32 result = (a - min) / (max - min);
    return result;
}

u8 g_memory[32*1024*1024];

#define Noise_Size 512

static f32 value_noise_2d(int x, int y, f32 *noise, u32 noise_size){
    x = (x % noise_size);
    y = (y % noise_size);
    f32 result = noise[x + y  * noise_size];
    return result;
}

static f32 smoothstep(f32 a, f32 b, f32 t){
    // Smoothstep adapted from the following source:
    // https://thebookofshaders.com/glossary/?search=smoothstep
    f32 c = clampf((t - a) / (b - a), 0.0f, 1.0f);
    f32 result = c *c * (3.0f - 2.0f * c);
    return result;
}

static f32 noise_2d(f32 x, f32 y, f32 *noise, u32 noise_size){
    int px = (int)x;
    int py = (int)y;

    //
    // Lerping the change between bottom corners and top corners and then lerping the
    // result of both is bilinear interpolation. Using smoothstep to ease the interpolant
    // helps the changes look more gradual than passing them directly to lerp.
    // Based on the Acerola video "What is Perlin Noise?".
    //
    f32 bottom0 = value_noise_2d(x, y, noise, noise_size);
    f32 bottom1 = value_noise_2d(x+1, y, noise, noise_size);
    f32 bottom  = lerp(bottom0, bottom1, smoothstep(0, 1, x - floor(x)));

    f32 top0 = value_noise_2d(x, y+1, noise, noise_size);
    f32 top1 = value_noise_2d(x+1, y+1, noise, noise_size);
    f32 top  = lerp(top0, top1, smoothstep(0, 1, x - floor(x)));

    f32 result = lerp(bottom, top, smoothstep(0, 1, y - floor(y)));
    return result;
}

#define Octives_Count 5

static void gen_grass(Img_Pixels dest, Buffer *memory){
    Buffer memory_restore = *memory;

    Xorshift32 rng = xs32_seed(3453789);

    // First, we generate some white noise. This is so that we can have a deterministic noise
    // function, where given two points "x" and "y" you will always get the same value between 0
    // and 1.
    f32 *noise = buffer_push_array(f32, memory, Noise_Size*Noise_Size);
    for_count(u32, i, Noise_Size*Noise_Size){
        noise[i] = random_percent(xs32_rand(&rng));
    }

    Vec4 grass_color_light = v4(0.10f, 0.68f, 0.32f, 1);
    Vec4 grass_color_dark  = v4(0.18f, 0.40f, 0.22f, 1);

    Vec4 ground_color_light  = v4(0.28f, 0.18f, 0.12f, 1);
    Vec4 ground_color_dark   = v4(0, 0, 0, 1);

    f32 *heightmap = buffer_push_array(f32, memory, dest.width*dest.height);

    /*f32 scales[4]   = {0.25f, 0.25f, 0.75f, 0.1f};*/
    /*f32 weights[4]  = {0.1f, 0.25f, 0.5f, 0.25};*/

    f32 scales[Octives_Count]  = {0.01f, 0.05f, 0.5f, 0.75f, 1.0f};
    f32 weights[Octives_Count] = {0.3f, 0.24f, 0.6f, 0.8f, 0.1f};

    for_count(u32, py, dest.height){
        for_count(u32, px, dest.width){
            f32 x = (f32)px;
            f32 y = (f32)py;

#if 0
            // Doing a random offset looks pretty nice here. If we do it where the offset is only
            // positive along the Y-axis, that means we could make it looks like grass is
            // "growing" in that direction.
            f32 xoffset = random_percent(xs32_rand(&rng));
            f32 yoffset = -random_percent(xs32_rand(&rng));
            f32 t = noise_2d((x + xoffset)*0.4f, (y+yoffset)*0.28f, noise, Noise_Size);
            heightmap[px + py * dest.width] = t;
#else
            f32 xoffset = random_percent(xs32_rand(&rng));
            f32 yoffset = -random_percent(xs32_rand(&rng));

            f32 total_height  = 0;
            f32 total_weights = 0;
            for_count(u32, i, Octives_Count){
                f32 s = scales[i];
                f32 w = weights[i];

                // NOTE: Counter intuitively, with this noise function, the larger the scaling
                // factor the smaller the density of white noise pixels we sample. By scaling up
                // the y-axis, we "pinch" the results to make details more obvious along the X
                // axis, giving the result a windswept look.
                total_height += w*noise_2d(x*s, y*s*2.8f, noise, Noise_Size);
                total_weights += w;
            }

            f32 t = total_height / total_weights;
            heightmap[px + py * dest.width] = t;
#endif

        }
    }

    // Apply colors based on height data
    for_count(u32, y, dest.height){
        for_count(u32, x, dest.width){
            u32 *pixel = &dest.data[x + y * dest.width];

#if 1
            f32 h = heightmap[x + y * dest.width];
            Vec4 color = v4_lerp(grass_color_light, grass_color_dark, h);
            *pixel = color_from_v4(color);
#else
            f32 h = heightmap[x + y * dest.width];
            f32 threshold = 0.10f;
            if(h > threshold){
                f32 t = normalized_range_clamp(h, threshold, 1);
                Vec4 color = v4_lerp(grass_color_light, grass_color_dark, t);
                *pixel = color_from_v4(color);
            }
            else{
                f32 t = normalized_range_clamp(t, 0, threshold);
                Vec4 color = v4_lerp(ground_color_light, ground_color_dark, t);
                *pixel = color_from_v4(color);
            }
#endif
        }
    }

    *memory = memory_restore;
}

int main(){
    Buffer memory = {g_memory, Array_Len(g_memory)};

    u32 target_w = 1920;
    u32 target_h = 1080;

    Img_Pixels pixels = {buffer_push_array(u32, &memory, target_w*target_h), target_w, target_h};
    gen_grass(pixels, &memory);

    img_save_tga("gass.tga", pixels.width, pixels.height, pixels.data, &memory);

    return 0;
}
