//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2026
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

/*
TODO:
    - Title screen.
    - Highscore table.
    - Speed should increase as new parts are added.
    - Procedural color selection for snake parts.
    - Gradually tapering snake part sizes.
*/

#include "cbed/common.h"
#include "cbed/font.h"
#include "cbed/files.h"
#include "cbed/img.h"
#include "cbed/random.h"

typedef struct{
    Vec2 p;
    f32  radius;
    u32  color;
    bool can_collide;
} Snake_Part;

typedef struct{
    u32        parts_count;
    Snake_Part parts[128];
    Vec2       vel;
} Snake;

typedef struct{
    Buffer *memory;
    Buffer *temp;
    Font   *font;
    Blit_Canvas grass;
    Snake snake;
    f32 screen_w;
    f32 screen_h;
    int score;
    int high_score;
    Xorshift32 rng;
    Vec2 food_pos;
    f32  food_radius;
} Game;

static void spawn_food(Game *s){
    f32 px = random_f32_between(xs32_rand(&s->rng), s->food_radius, s->screen_w - s->food_radius);
    f32 py = random_f32_between(xs32_rand(&s->rng), s->food_radius, s->screen_h - s->food_radius);
    s->food_pos = v2(px, py);
}

static void snake_add_part(Snake *snake){
    if(snake->parts_count < Array_Len(snake->parts)){
        Snake_Part *part = &snake->parts[snake->parts_count++];
        part->can_collide = false;
    }
}

static void game_restart(Game *s){
    s->score = 0;

    Snake *snake = &s->snake;
    snake->parts_count = 0;
    snake_add_part(snake); // Add the head
    s->food_radius = 0.018f*s->screen_w;

    spawn_food(s);
}

static Blit_Canvas pixels_to_canvas(Img_Pixels pixels){
    Blit_Canvas result = {
        pixels.data,
        pixels.width,
        pixels.height,
        pixels.width
    };
    return result;
}

static bool game_init(Game *s, f32 screen_w, f32 screen_h, Buffer *memory, Buffer *temp){
    s->screen_w = screen_w;
    s->screen_h = screen_h;
    s->memory = memory;
    s->temp  = temp;

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

    const char *grass_file_path = "/grass.tga";
    String grass_data = file_read_into_memory(grass_file_path, temp);
    Img_Pixels grass_pixels = img_load_tga_from_memory(grass_file_path, grass_data.text, grass_data.size, memory);
    s->grass = pixels_to_canvas(grass_pixels);
    if(!s->grass.pixels){
        return false;
    }

    Vec2 screen_center = v2(0.5f*s->screen_w, 0.5f*s->screen_h);

    for_count(u32, i, Array_Len(s->snake.parts)){
        Snake_Part *part = &s->snake.parts[i];

        part->p = screen_center;
        part->radius = 0.035f*s->screen_h;
        part->color = 0xffff0000;
    }

    // TODO: Use time as the seed?
    s->rng = xs32_seed(748479887);

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
    int max_x = MIN(center.x + radius+1, (f32)dest->width);
    int max_y = MIN(center.y + radius+1, (f32)dest->height);

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

static void blit_background(Blit_Dest *img_dest, Blit_Canvas img_src){
    u32 *dest = img_dest->pixels;
    u32 *src  = img_src.pixels;

    u32 width  = MIN(img_src.width, img_dest->width);
    u32 height = MIN(img_src.height, img_dest->height);
    u32 src_advance  = img_src.stride - width;
    u32 dest_advance = img_dest->stride - width;

    for_count(u32, y, height){
        for_count(u32, x, width){
            *dest++ = *src++;
        }
        src  += src_advance;
        dest += dest_advance;
    }
}

static void game_sim_and_draw(Game *s, Blit_Dest *dest, Vec2 mouse, f32 dt){
    Buffer *temp = s->temp;

    //
    // Simulate the snake.
    //
    Snake *snake = &s->snake;

    Snake_Part *head = &snake->parts[0];
    Vec2 snake_dir = v2_normalize(v2_sub(mouse, head->p));
    f32 snake_speed = 0.75f*(f32)s->screen_w;
    Vec2 snake_accel = v2_muls(snake_dir, snake_speed*dt);
    snake->vel = v2_add(snake->vel, snake_accel);
    snake->vel = v2_sub(snake->vel, v2_muls(snake->vel, 0.05f)); // Estimate friction
    head->p = v2_add(head->p, v2_muls(snake->vel, dt));

    if(circles_overlap(head->p, head->radius, s->food_pos, s->food_radius)){
        s->score++;
        spawn_food(s);
        snake_add_part(snake);
    }

    for_range(u32, i, 1, Array_Len(snake->parts)){
        Snake_Part *parent = &snake->parts[i-1];
        Snake_Part *part = &snake->parts[i];
        f32 target_dist = parent->radius + part->radius;
        if(v2_dist_sq(part->p, parent->p) > squared(target_dist)){
            Vec2 dir = v2_normalize(v2_sub(part->p, parent->p));
            part->p = v2_add(parent->p, v2_muls(dir, target_dist));
        }

        bool part_overlaps_head = circles_overlap(part->p, part->radius, head->p, head->radius);
        if(part->can_collide){
            // We only want to check for collisions if this part has actually been spawned
            // and as long as it's not the second snake part. There is no legitimate way
            // for the snake head to hit the very next part.
            if(part_overlaps_head && i < snake->parts_count && i > 1){
                // TODO: Game over!
                assert(0);
            }
        }
        else{
            // This is a small gameplay tweak in favor of the player. If a new snake part spawns
            // underneath the snake head, that won't count as a collision until the head moves
            // away from the part.
            if(!part_overlaps_head){
                part->can_collide = true;
            }
        }
    }

    //
    // Render the game.
    //
    Font *font = s->font;

    f32 target_x = ((f32)mouse.x) / (f32)dest->width;
    f32 target_y = ((f32)mouse.y) / (f32)dest->height;

    /*draw_gradiant((Blit_Canvas *)dest, target_x, target_y);*/
    /*blit_sprite(dest, s->grass, v3(0, 0, 0));*/
    /*blit_copy(dest, s->grass, v3(0, 0, 0));*/
    blit_background(dest, s->grass);

    f32 pad = 16;
    Vec3 baseline = v3(pad, font->cap_height + pad, 0);
    blit_text(dest, baseline, font, gen_string("Score: %d", temp, s->score));

    /*baseline.y += font->line_gap;*/
    /*blit_text(dest, baseline, font, gen_string("Mouse: %d, %d", temp, (int)mouse.x, (int)mouse.y));*/

    baseline.y += font->line_gap;
    blit_text(dest, baseline, font, gen_string("dt: %.4f", temp, dt));

    blit_gradient_ball(dest, s->food_pos, s->food_radius, 0xff42c3ff, 0xff172cde);

    for_count(u32, i, snake->parts_count){
        Snake_Part *part = &snake->parts[i];
        blit_gradient_ball(dest, part->p, part->radius, part->color, 0xff000000);
    }

    blit_rect(dest, (Rect){mouse, v2(2, 20)}, 0, 0xffffffff);
    blit_rect(dest, (Rect){mouse, v2(20, 2)}, 0, 0xffffffff);
}
