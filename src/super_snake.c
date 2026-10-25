//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2026
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

#include "cbed/common.h"
#include "cbed/font.h"
#include "cbed/files.h"
#include "cbed/img.h"
#include "cbed/random.h"

#define Color_White 0xffffffff

typedef enum {
    Game_Mode_Menu,
    Game_Mode_Play,
    Game_Mode_GameOver,
    Game_Mode_Scores,
    Game_Mode_Credits,
} Game_Mode;

typedef struct{
    Vec2 p;
    f32  radius;
    u32  color;
    bool can_collide;
} Snake_Part;

typedef struct{
    u32        parts_count;
    Snake_Part parts[128];
    Vec2       facing;
} Snake;

typedef struct{
    Buffer *memory;
    Buffer *temp;
    Font   *font;
    Blit_Canvas grass;
    Snake snake;

    Game_Mode game_mode;
    f32 screen_w;
    f32 screen_h;
    Xorshift32 rng;
    f32 t;
    Vec2 menu_focus;
    Vec2 food_pos;
    f32  food_radius;
    bool debug_mode;

    u32 score;
    u32 highscores[10];
    u32 *new_score_slot;
} Game;

static void spawn_food(Game *s){
    f32 px = random_f32_between(xs32_rand(&s->rng), s->food_radius, s->screen_w - s->food_radius);
    f32 py = random_f32_between(xs32_rand(&s->rng), s->food_radius, s->screen_h - s->food_radius);
    s->food_pos = v2(px, py);
}

static void snake_add_part(Snake *snake){
    if(snake->parts_count < Array_Len(snake->parts)){
        snake->parts_count++;
    }
}

static void game_restart(Game *s){
    s->score = 0;
    s->new_score_slot = NULL;

    Snake *snake = &s->snake;
    Vec2 screen_center = v2(0.5f*s->screen_w, 0.5f*s->screen_h);
    for_count(u32, i, Array_Len(snake->parts)){
        Snake_Part *part = &snake->parts[i];

        part->p = screen_center;
        part->can_collide = false;
    }

    snake->parts_count = 0;
    snake_add_part(snake); // Add the head
    s->food_radius = 0.018f*s->screen_w;
    s->game_mode = Game_Mode_Play;

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

    f32 max_part_size = 0.035f*s->screen_h;
    f32 min_part_size = max_part_size*0.25f;

    Xorshift32 snake_rng = xs32_seed(827496172);

    for_count(u32, i, Array_Len(s->snake.parts)){
        Snake_Part *part = &s->snake.parts[i];

        f32 t = ((f32)i) / (f32)Array_Len(s->snake.parts);
        part->radius = lerp(min_part_size, max_part_size, 1.0f-t);

        if(i > 0){
            f32 r = random_percent(xs32_rand(&snake_rng));
            f32 g = random_percent(xs32_rand(&snake_rng));
            f32 b = random_percent(xs32_rand(&snake_rng));
            part->color = color_from_v4(v4(r, g, b, 1));
        }
        else{
            part->color = color_from_v4(v4(0, 0, 1, 1));
        }
    }

    s->game_mode = Game_Mode_Menu;
    s->menu_focus = v2(0.5f, 0.5f);

#ifdef VM_Target
    s->debug_mode = true;
#endif

    // TODO: Use time as the seed?
    s->rng = xs32_seed(748479887);

    return true;
}

static f32 dist(f32 x, f32 y){
    f32 result = sqrt(x*x + y*y);
    return result;
}

static void draw_gradiant(Blit_Canvas *dest, f32 target_x, f32 target_y, f32 t){
    f32 aspect_ratio = (f32)dest->width / (f32)dest->height;

    f32 scale = 8.0f + (sin(t));

    u32 line_advance = dest->stride - dest->width;
    u32 *pixels = dest->pixels;
    for_count(u32, y, dest->height){
        for_count(u32, x, dest->width){
            f32 dx = ((f32)x) / (f32)dest->width;
            f32 dy = ((f32)y) / (f32)dest->height;


            f32 value = 1.0f-dist(aspect_ratio*(dx - target_x), dy - target_y)*scale;
            u32 b = (u8)(value * 255.0f);

            u32 color = 0xff440000 | (b << 16);
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
    // NOTE: The result would be better if done in linear RGB-space.
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

static void game_sim_and_draw(Game *s, Blit_Dest *dest, Vec2 mouse, bool mouse_left_clicked, f32 dt){
    Buffer *temp = s->temp;

    Snake *snake = &s->snake;
    s->t += dt;

    ////
    //
    // Simulate the game
    //
    ////
    switch(s->game_mode){
        default: assert(0);

        case Game_Mode_Menu:{
            if(mouse_left_clicked){
                game_restart(s);
            }
        } break;

        case Game_Mode_GameOver:{
            if(mouse_left_clicked){
                s->game_mode = Game_Mode_Scores;
            }
        } break;

        case Game_Mode_Scores:{
            if(mouse_left_clicked){
                s->game_mode = Game_Mode_Menu;
            }
        } break;

        case Game_Mode_Play:{
            //
            // Simulate the snake.
            //
            f32 snake_speed_min = 0.25f*(f32)s->screen_w;
            f32 snake_speed_max = 2.5f*snake_speed_min;
            f32 snake_speed_t = ((f32)snake->parts_count) / (f32)Array_Len(s->snake.parts);
            f32 snake_speed = lerp(snake_speed_min, snake_speed_max, snake_speed_t);

            Snake_Part *head = &snake->parts[0];
            if(v2_dist_sq(head->p, mouse) > squared(head->radius)){
                Vec2 snake_target_dir = v2_normalize(v2_sub(mouse, head->p));
                snake->facing = snake_target_dir;
            }

            Vec2 snake_delta = v2_muls(snake->facing, snake_speed*dt);
            Vec2 head_target_p = v2_add(head->p, snake_delta);

            // Prevent the head from being able to back up into the very next part (the "neck")
            if(snake->parts_count > 1){
                Snake_Part *neck = &snake->parts[1];
                f32 min_dist = head->radius + neck->radius;
                if(v2_dist_sq(head_target_p, neck->p) < squared(min_dist)){
                    Vec2 dir = v2_normalize(v2_sub(head_target_p, neck->p));
                    head_target_p = v2_add(neck->p, v2_muls(dir, min_dist+snake_speed*dt));
                }
            }

            head->p = head_target_p;


            if(circles_overlap(head->p, head->radius, s->food_pos, s->food_radius)){
                s->score++;
                spawn_food(s);
                snake_add_part(snake);
            }

            for_range(u32, part_index, 1, Array_Len(snake->parts)){
                Snake_Part *parent = &snake->parts[part_index-1];
                Snake_Part *part = &snake->parts[part_index];
                f32 target_dist = parent->radius + part->radius;
                if(v2_dist_sq(part->p, parent->p) > squared(target_dist)){
                    Vec2 dir = v2_normalize(v2_sub(part->p, parent->p));
                    part->p = v2_add(parent->p, v2_muls(dir, target_dist));
                }

                if(part->can_collide){
                    // Give the player a little leaway when it comes to colliding with it's own body.
                    f32 head_test_radius = head->radius - head->radius*0.35f;

                    // We only want to check for collisions if this part has actually been spawned
                    // and as long as it's not the second snake part. There is no legitimate way
                    // for the snake head to hit the very next part.
                    bool overlaps_head = circles_overlap(part->p, part->radius, head->p, head_test_radius);
                    if(overlaps_head && part_index < snake->parts_count && part_index > 1){
                        s->game_mode = Game_Mode_GameOver;

                        // Search to see if we can add the current score to the highscores list.
                        for_count(u32, i, Array_Len(s->highscores)){
                            if(s->score >= s->highscores[i]){
                                // Bump lower scores further down
                                if(i+1 < Array_Len(s->highscores)){
                                    memmove(&s->highscores[i+1], &s->highscores[i],
                                    (Array_Len(s->highscores) - i - 1)*sizeof(u32));
                                }

                                s->highscores[i]  = s->score;
                                s->new_score_slot = &s->highscores[i];
                                break;
                            }
                        }
                        break;
                    }
                }
                else{
                    // This is a small gameplay tweak in favor of the player. If a new snake part spawns
                    // underneath the snake head, that won't count as a collision until the head moves
                    // away from the part.
                    if(!circles_overlap(part->p, part->radius, head->p, head->radius)){
                        part->can_collide = true;
                    }
                }
            }
        } break;
    }

    ////
    //
    // Render the game.
    //
    ////
    Font *font = s->font;

    Vec2 screen_center = v2(0.5f*s->screen_w, 0.5f*s->screen_h);

    f32 target_x = ((f32)mouse.x) / (f32)dest->width;
    f32 target_y = ((f32)mouse.y) / (f32)dest->height;

    f32 hud_pad = 16;
    Vec3 baseline = v3(hud_pad, font->cap_height + hud_pad, 0);
    if(s->game_mode != Game_Mode_Menu){
        blit_background(dest, s->grass);

        if(s->game_mode == Game_Mode_Play){
            blit_text(dest, baseline, font, gen_string("Score: %d", temp, s->score));
            baseline.y += font->line_gap;
        }

        blit_gradient_ball(dest, s->food_pos, s->food_radius, 0xff42c3ff, 0xff172cde);

        for_count(u32, i, snake->parts_count){
            Snake_Part *part = &snake->parts[i];
            blit_gradient_ball(dest, part->p, part->radius, part->color, 0xff000000);
        }
    }
    else{
        f32 x = lerp(s->menu_focus.x, target_x, 0.2f);
        f32 y = lerp(s->menu_focus.y, target_y, 0.2f);

        draw_gradiant((Blit_Canvas *)dest, x, y, 1);
    }

    if(s->debug_mode){
        blit_text(dest, baseline, font, gen_string("dt: %.4f", temp, dt));
        baseline.y += font->line_gap;
    }

    blit_rect(dest, (Rect){mouse, v2(2, 20)}, 0, 0xffffffff);
    blit_rect(dest, (Rect){mouse, v2(20, 2)}, 0, 0xffffffff);

    switch(s->game_mode){
        default: break;

        case Game_Mode_Menu:{
            String title = str_lit("Super Snake");
            f32 tw = font_get_text_width(font, title);
            Vec3 baseline = v3(screen_center.x - tw*0.5f, screen_center.y*0.5f, 0);
            blit_text(dest, baseline, font, title);
        } break;

        case Game_Mode_GameOver:{
            String msg = str_lit("Game Over!");
            f32 tw = font_get_text_width(font, msg);
            Vec3 baseline = v3(screen_center.x - tw*0.5f, screen_center.y, 0);
            blit_text(dest, baseline, font, msg);
        } break;

        case Game_Mode_Scores:{
            String title = str_lit("Highscores:");
            f32 tw = font_get_text_width(font, title);
            Vec3 baseline = v3(screen_center.x - tw*0.5f, screen_center.y*0.5f, 0);
            blit_text(dest, baseline, font, title);
            baseline.y += font->line_gap;

            f32 table_width = 400;
            baseline.x = screen_center.x - table_width*0.5f;
            for_count(u32, score_index, Array_Len(s->highscores)){
                u32 *slot = &s->highscores[score_index];
                if(slot == s->new_score_slot){
                    f32 t = absf(0.8f*cos(0.5f*s->t*TAU));
                    Vec4 color = v4_lerp(v4(1, 1, 0, 1), v4(1, 1, 1, 1), t);
                    dest->tint = color_from_v4(color);
                }
                else{
                    dest->tint = Color_White;
                }
                blit_text(dest, baseline, font, gen_string("%02d", temp, score_index+1));

                u32 score = *slot;
                if(score > 0){
                    String score_text = gen_string("%d", temp, score);
                    f32 tw = font_get_text_width(font, score_text);
                    Vec3 p = v3(baseline.x + table_width - tw, baseline.y, baseline.z);
                    blit_text(dest, p, font, score_text);
                }
                baseline.y += font->line_gap;
            }

            // NOTE: Ensure we don't accidentally leave sprite tinting turned on.
            dest->tint = Color_White;
        } break;
    }
}
