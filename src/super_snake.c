//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2026
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

#include "cbed/common.h"
#include "cbed/font.h"
#include "cbed/files.h"

typedef struct{
    Vec2 p;
    f32  radius;
    u32  color;
} Ball;

typedef struct{
    u32  parts_count;
    Ball parts[256];
    Vec2 vel;
} Snake;

typedef struct{
    Buffer *memory;
    Font   *font;
    Snake snake;
    f32 screen_w;
    f32 screen_h;
    int score;
    int high_score;
} Game;

static void snake_add_part(Snake *snake, Vec2 p, f32 radius, u32 color){
    assert(snake->parts_count <  Array_Len(snake->parts));
    Ball *part = &snake->parts[snake->parts_count++];
    part->p      = p;
    part->radius = radius;
    part->color  = color;
}

static void game_restart(Game *s){
    s->score = 0;

    Snake *snake = &s->snake;
    snake_add_part(snake, v2(0.5f*s->screen_w, 0.5f*s->screen_h), 0.035f*s->screen_h, 0xffff0000);
}

static bool game_init(Game *s, f32 screen_w, f32 screen_h, Buffer *memory){
    s->screen_w = screen_w;
    s->screen_h = screen_h;
    s->memory = memory;

    String font_data = file_read_into_memory("/font_en.fnt", memory);
    if(!font_data.size){
        return false;
    }
    s->font = (Font *)font_data.text;

    Font *font = s->font;
    u8 *font_base = (u8 *)font;
    Font_Info *info = (Font_Info *)&font_base[font->font_info_offset];
    const char *font_name = (char *)(info+1);
    fmt_msg("Loaded font '{0}' at {1}px\n", fmt_cstr(font_name), fmt_i(info->height));

    game_restart(s);

    return true;
}

static f32 dist(f32 x, f32 y){
    f32 result = sqrt(x*x + y*y);
    return result;
}

static void draw_gradiant(Blit_Canvas *dest, f32 target_x, f32 target_y){
    f32 aspect_ratio = (f32)dest->width / (f32)dest->height;

    u32 line_advance = dest->stride - dest->width;
    u32 *pixels = dest->pixels;
    for_count(u32, y, dest->height){
        for_count(u32, x, dest->width){
            f32 dx = ((f32)x) / (f32)dest->width;
            f32 dy = ((f32)y) / (f32)dest->height;

            f32 value = 1.0f-dist(aspect_ratio*(dx - target_x), dy - target_y)*8.0f;
            u32 b = (u8)(value * 255.0f);

            u32 color = 0xff000000 | (b << 0);
            *pixels = color;
            pixels++;
        }
        pixels += line_advance;
    }
}

static String gen_string(const char *fmt, Buffer *buffer, ...){
    String result = {&buffer->data[buffer->used], 0};

    // When in Rome...
    va_list args;
    va_start(args, buffer);
    int bytes = vsnprintf(result.text, buffer->size - buffer->used, fmt, args);
    va_end(args);

    if(bytes != -1){
        result.size = bytes;
        buffer->used += bytes + 1; // Include the null-terminator
    }

    return result;
}

static u32 lerp_colors(u32 a, u32 b, f32 t){
    // TODO: This should probably be done in linear RGB-space.
    Vec4 av = v4(
        ((a >> 16) & 0xff), ((a >>  8) & 0xff),
        ((a >>  0) & 0xff), ((a >> 24) & 0xff)
    );

    Vec4 bv = v4(
        ((b >> 16) & 0xff), ((b >>  8) & 0xff),
        ((b >>  0) & 0xff), ((b >> 24) & 0xff)
    );

    Vec4 v = v4_lerp(av, bv, t);
    u32 result = ((u32)v.a) << 24 | ((u32)v.r) << 16
               | ((u32)v.g) <<  8 | ((u32)v.b) <<  0;
    return result;
}

static void blit_gradient_ball(Blit_Dest *dest, Vec2 center, f32 radius, u32 color_a, u32 color_b){
    int min_x = MAX(center.x - radius, 0.0f);
    int min_y = MAX(center.y - radius, 0.0f);
    int max_x = MIN(center.x + radius, (f32)dest->width);
    int max_y = MIN(center.y + radius, (f32)dest->height);

    for_range(int, y, min_y, max_y){
        for_range(int, x, min_x, max_x){
            Vec2 p = v2(x, y);
            if(v2_dist_sq(center, p) < squared(radius)){
                // TODO: Is there a way to interpolate the colors without using a sqrt?
                f32 t = v2_dist(center, p) / radius;
                u32 color = lerp_colors(color_a, color_b, t);
                dest->pixels[x + y * dest->stride] = color;
            }
        }
    }
}

static void game_sim_and_draw(Game *s, Blit_Dest *dest, Vec2 mouse, f32 dt, Buffer *temp){
    // Simulate the snake.
    Snake *snake = &s->snake;

    Ball *head = &snake->parts[0];
    Vec2 snake_dir = v2_normalize(v2_sub(mouse, head->p));
    f32 snake_speed = 0.1f*(f32)s->screen_w;
    Vec2 snake_accel = v2_muls(snake_dir, snake_speed*dt);
    snake->vel = v2_add(snake->vel, snake_accel);
    snake->vel = v2_sub(snake->vel, v2_muls(snake->vel, 0.05f)); // Estimate friction
    head->p = v2_add(head->p, v2_muls(snake->vel, dt));

    // Render the game.
    Font *font = s->font;

    f32 target_x = ((f32)mouse.x) / (f32)dest->width;
    f32 target_y = ((f32)mouse.y) / (f32)dest->height;
    draw_gradiant((Blit_Canvas *)dest, target_x, target_y);

    f32 pad = 16;
    Vec3 baseline = v3(pad, font->cap_height + pad, 0);
    blit_text(dest, baseline, font, gen_string("Score: %d", temp, s->score));

    baseline.y += font->line_gap;
    blit_text(dest, baseline, font, gen_string("Mouse: %d, %d", temp, mouse.x, mouse.y));

    baseline.y += font->line_gap;
    blit_text(dest, baseline, font, gen_string("dt: %.2f", temp, dt));

    /*Rect cursor = {mouse, v2(20, 20)};*/
    /*blit_rect(dest, cursor, 0, 0xff00ffff);*/

    for_count(u32, i, snake->parts_count){
        Ball *part = &snake->parts[i];
        blit_gradient_ball(dest, part->p, part->radius, part->color, 0xff000000);
    }
}
