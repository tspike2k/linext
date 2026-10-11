/*
Authors:   tspike (github.com/tspike2k)
Copyright: Copyright (c) 2026
License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
*/

#include "font.h"

Cbed_API bool font_is_valid(Font* font, size_t memory_size){
    bool result = memory_size > sizeof(Font) && memory_size >= font->expected_size;
    return result;
}

Cbed_API f32 font_get_kerning_advance(Font* font, u32 prev_codepoint, u32 codepoint){
    u8 *base = (u8 *)font;
    Font_Kerning_Pair *kerning_pairs = (Font_Kerning_Pair *)&base[font->kerning_pairs_offset];
    float *kerning_advance = (float *)&base[font->kerning_advance_offset];

    f32 result = 0.0f;
    for_count(u32, i, font->kerning_pairs_count){
        Font_Kerning_Pair *entry = &kerning_pairs[i];
        if(entry->a == prev_codepoint && entry->b == codepoint){
            result = kerning_advance[i];
            break;
        }
    }

    return result;
}

Cbed_API Font_Glyph* font_get_glyph(Font* font, u32 codepoint){
    // IMPORTANT! Code that calls this function should test to ensure the font contains glyphs.
    assert(font->glyphs_count > 0);

    u8 *base           = (u8 *)font;
    u32 *codepoints    = (u32 *)&base[font->glyphs_codepoint_offset];
    Font_Glyph *glyphs = (Font_Glyph *)&base[font->glyphs_offset];

    // The first glyph is always the Null Glyph.
    Font_Glyph *result = &glyphs[0];

    for_count(u32, i, font->glyphs_count){
        if(codepoints[i] == codepoint){
            result = &glyphs[i];
            break;
        }
    }

    return result;
}

Cbed_API f32 font_get_text_width(Font* font, const char *text, size_t text_len){
    if(font->glyphs_count == 0) return 0;

    u32 prev_codepoint = 0;
    f32 result = 0;
    for_count(size_t, i, text_len){
        char c = text[i];
        // TODO: Account for line endings?

        Font_Glyph *glyph   = font_get_glyph(font, c);
        float kerning = font_get_kerning_advance(font, prev_codepoint, c);
        result += kerning;
        result += (float)glyph->advance;
        prev_codepoint = c;
    }
    return result;
}

#ifdef FONT_BUILDER

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftstroke.h>
#include "img.h"
#include "math.h"
#include "files.h"

struct Font_Builder{
    Buffer*      memory;
    Font_Info    font_info;

    u32          codepoints_count;
    u32         *codepoints;
    Font_Glyph  *glyphs;
    u32        **glyph_pixels;
    u32         *atlas;
    u32          atlas_w;
    u32          atlas_h;

    FT_Library lib;
    FT_Face    face;
    FT_Stroker stroker;
};

typedef struct{
    u32 x, y, w, h;
} Font_Rect;


#ifdef OS_Linux
const char * font__search_paths[] ={
    "/usr/share/fonts",
};
#else
#error No font search paths for target OS.
#endif

// TODO: Since this is rather nuanced, should we expose this to the user?
static const char *font__get_font_path(const char *font_file_name, Buffer* memory){
    char *result = NULL;

    for(u32 i = 0; !result && i < Array_Len(font__search_paths); i++){
        const char *path = font__search_paths[i];

        File_Walker walker;
        file_walker_begin(&walker, path);
        while(file_walker_advance(&walker)){
            if(walker.file_type == File_Type_Directory){
                file_walker_enter_directory(&walker);
            }
            else if(walker.file_type == File_Type_File){
                if(strcmp(walker.file_name, font_file_name) == 0){
                    String path = file_walker_make_path(&walker, memory);
                    result = path.text;
                    break;
                }
            }
        }

        file_walker_end(&walker);
    }

    return result;
}

Cbed_API Font_Builder *font_builder_begin(Buffer *buffer){
    Font_Builder *result = buffer_push_type(Font_Builder, buffer);

    result->memory = buffer;
    if(FT_Init_FreeType(&result->lib) != 0){
        fmt_msg("Error! Failed to initialize Freetype2.\n");
        return false;
    }

    return result;
}

Cbed_API void font_builder_end(Font_Builder *s){
    if(s->lib) FT_Done_FreeType(s->lib);
}

static u32 font__blend_colors_premultiplied_alpha(u32 source, u32 dest){
    // Blending code adapted from Handmade Hero.
    // TODO: Cite which day of Handmade Hero it was from

    // TODO: Gamma corrected colors? See here for more information:
    // https://www.youtube.com/watch?v=fVyzTKCfchw&feature=youtu.be&t=3275
    Vec4 s = {{
        (source >> 16) & 0xff, (source >>  8) & 0xff,
        (source >>  0) & 0xff, (source >> 24) & 0xff
    }};

    Vec4 d = {{
        (dest >> 16) & 0xff, (dest >>  8) & 0xff,
        (dest >>  0) & 0xff, (dest >> 24) & 0xff
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

static void font__blit_glyph(FT_BitmapGlyph bitmap_glyph, u32 dest_w, u32 dest_h, u32* dest, u32 target_color, u32 offset_x, u32 offset_y){
    u32 w = bitmap_glyph->bitmap.width;
    u32 h = bitmap_glyph->bitmap.rows;

    // TODO: Better blitting code
    for(u32 y = 0; y < h; y++){
        for(u32 x = 0; x < w; x++){
            u32 alpha = bitmap_glyph->bitmap.buffer[x + y*w];
            u32 color = premultiply_alpha((target_color & 0x00ffffff) | (alpha << 24));

            u32 pixel_index = offset_x+x + (offset_y+y)*dest_w;
            assert(pixel_index < dest_w*dest_h);
            u32 *pixel = &dest[pixel_index];
            *pixel = font__blend_colors_premultiplied_alpha(color, *pixel);
        }
    }
}

static FT_BitmapGlyph font__rasterize_glyph(FT_Face face, FT_Stroker stroker, u32 codepoint, u32 stroke){
    // TODO: Error handling? We at least need to know if the glyph was sucessfully found or not.
    FT_Load_Char(face, codepoint, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
    FT_Glyph glyph_info;
    FT_Get_Glyph(face->glyph, &glyph_info);
    assert(glyph_info->format == FT_GLYPH_FORMAT_OUTLINE);

    if(stroke > 0)
        FT_Glyph_StrokeBorder(&glyph_info, stroker, false, true);

    FT_Glyph_To_Bitmap(&glyph_info, FT_RENDER_MODE_NORMAL, NULL, 1);
    FT_BitmapGlyph result = (FT_BitmapGlyph)glyph_info;
    return result;
}

static bool font__generate_glyph(Font_Builder *s, u32 codepoint, u32 index){
    // TODO: Better error handling!
    bool succeeded = true;

    FT_Face     face       = s->face;
    FT_Stroker  stroker    = s->stroker;
    Font_Info*  font_entry = &s->font_info;

    s->codepoints[index] = codepoint;
    Font_Glyph *glyph = &s->glyphs[index];

    if(!char_is_whitespace((char)codepoint)){
        FT_BitmapGlyph bitmap_glyph = font__rasterize_glyph(face, stroker, codepoint, font_entry->stroke);

        // Copy glyph metrics
        //
        // NOTE: The offset values are added to the pen position to correctly align the glyph bitmap
        // when rendering text. The x-offset is the left-side bearing of the glyph. The y-offset
        // expects glyph bitmaps to be drawn from the bottom-left, with the y-axis growing upwards.
        // The value of the y-offset is the descender and will be negative for glyphs that extend
        // below the baseline.
        //
        // FT_BitmapGlyph.left:        left-side bearing
        // FT_BitmapGlyph.top:         top-side bearing (ascender?)
        // FT_BitmapGlyph.bitmap.rows: glyph pixel height
        glyph->advance    = ((u32)face->glyph->advance.x) >> 6;
        glyph->offset.x  = bitmap_glyph->left;
        glyph->offset.y  = -((float)(bitmap_glyph->bitmap.rows) - (float)(bitmap_glyph->top)) + (float)font_entry->stroke; // NOTE: We must cast before negation as the metrics are unsigned integers

        // Copy the rasterized glyph
        glyph->width  = bitmap_glyph->bitmap.width;
        glyph->height = bitmap_glyph->bitmap.rows;
        s->glyph_pixels[index] = buffer_push_array(u32, s->memory, glyph->width*glyph->height);
        u32 *pixels = s->glyph_pixels[index];

        u32 target_color = font_entry->stroke == 0 ? font_entry->fill_color : font_entry->stroke_color;
        font__blit_glyph(bitmap_glyph, glyph->width, glyph->height, pixels, target_color, 0, 0);

        // If a stroke is defined, now draw the fill.
        if(font_entry->stroke){
            u32 stroke_left = bitmap_glyph->left;
            u32 stroke_top  = bitmap_glyph->top;

            bitmap_glyph = font__rasterize_glyph(face, stroker, codepoint, 0);
            u32 fill_offset_x = bitmap_glyph->left - stroke_left;
            u32 fill_offset_y = stroke_top - bitmap_glyph->top; // In Freetype the Y-axis of bitmaps grows upwards, hence the flipped subtraction.

            font__blit_glyph(
                bitmap_glyph, glyph->width, glyph->height, pixels,
                font_entry->fill_color, fill_offset_x, fill_offset_y
            );

            glyph->offset.x += fill_offset_x;
            //glyph.offset.y += fill_offset_y; // TODO: Should we account for stroke on the y-axis?
        }
    }
    else{
        FT_Load_Char(face, codepoint, FT_LOAD_DEFAULT);
        glyph->advance    = ((u32)face->glyph->advance.x) >> 6;
    }

    return succeeded;
}

static void font__bake_glyphs_to_atlas(Font_Builder *s){
    // TODO: It might be better to use a bin-packing algorithm that grows the canvas if needed,
    // like the following:
    // https://jakesgordon.com/writing/bin-packing/
    u32 padding = 1;

    u32 max_w = 0;
    // Guess canvas size
    for_count(u32, i, s->codepoints_count){
        if(s->glyph_pixels[i]){
            Font_Glyph *glyph = &s->glyphs[i];
            max_w = MAX(max_w, glyph->width + padding*2);
        }
    }

    u32 target_w = round_up_power_of_two(sqrt(s->codepoints_count) * max_w); // TODO: Try rounding down instead
    u32 target_h = 0; // NOTE: The height is chosen dynamically

    // Layout glyphs on canvas (do this as a unique pass in case we later add expanding the canvas)
    Font_Rect *bounds = buffer_push_array(Font_Rect, s->memory, s->codepoints_count);
    u32 pen_x = padding;
    u32 pen_y = padding;
    u32 line_height = 0;
    for_count(u32, i, s->codepoints_count){
        if(s->glyph_pixels[i]){
            Font_Glyph *glyph = &s->glyphs[i];

            if(pen_x + glyph->width >= target_w - padding){
                pen_x = padding;
                pen_y += line_height + padding;
                line_height = 0;
            }
            assert(pen_x + glyph->width < target_w - padding);
            /*assert(pen_y + glyph->height < target_h - padding);*/

            Font_Rect *r = &bounds[i];
            r->x = pen_x;
            r->y = pen_y;
            r->w = glyph->width;
            r->h = glyph->height;

            pen_x += r->w + padding;
            line_height = MAX(r->h, line_height);

            target_h = MAX(line_height, pen_y + r->h + padding);
        }
    }
    target_h = round_up_power_of_two(target_h);

    s->atlas_w = target_w;
    s->atlas_h = target_h;
    s->atlas = buffer_push_array(u32, s->memory, s->atlas_w*s->atlas_h);

    // Blit glyph pixels and calculate UVs
    for_count(u32, i, s->codepoints_count){
        if(s->glyph_pixels[i]){
            Font_Rect r       = bounds[i];
            u32 *src = s->glyph_pixels[i];
            u32 *dest = &s->atlas[r.x + r.y * s->atlas_w];
            /*u32 *dest = s->atlas;*/
            u32  dest_stride = s->atlas_w - r.w;

            for_count(u32, y, r.h){
                for_count(u32, x, r.w){
                    *dest++ = *src++;
                }
                dest += dest_stride;
            }

            Font_Glyph *glyph = &s->glyphs[i];
            glyph->uv_min = (Vec2){
                ((float)r.x) / ((float)s->atlas_w),
                ((float)r.y) / ((float)s->atlas_h)
            };

            glyph->uv_max = (Vec2){
                ((float)(r.x + r.w)) / ((float)s->atlas_w),
                ((float)(r.y + r.h)) / ((float)s->atlas_h)
            };
        }
    }
}

Cbed_API String font_builder_generate(Font_Builder *s, Font_Info info, const char* font_file_name, u32 *user_codepoints, u32 user_codepoints_count){
    String result = {0};
    if(!s->lib) return result;

    const char *font_path = font__get_font_path(font_file_name, s->memory);

    if(FT_New_Face(s->lib, font_path, 0, &s->face) != 0){
        fmt_msg("Error: Unable to load font file {0}. Aborting...\n", fmt_cstr(font_file_name));
        return result;
    }

    FT_Set_Pixel_Sizes(s->face, 0, info.height);
    if(info.stroke > 0){
        FT_Stroker_New(s->lib, &s->stroker);
        FT_Stroker_Set(s->stroker, info.stroke*64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    }

    // See here for a discussion on calculating the line gap:
    // https://freetype.nongnu.narkive.com/MyeGsd2a/ft-vert-advance-on-line-break
    // https://stackoverflow.com/a/30793586
    FT_Face face = s->face;
    u32 internal_leading = (u32)((face->size->metrics.ascender - face->size->metrics.descender) >> 6) - face->size->metrics.y_ppem;
    u32 line_gap    = (u32)(face->size->metrics.height) >> 6;
    u32 cap_height  = (u32)((face->size->metrics.ascender >> 6) - internal_leading);
    u32 char_height = (u32)(face->bbox.yMax - face->bbox.yMin) >> 6;

    // We need codepoints for both the null glyph ('\0') and space (' '). Prepend these to
    // the list of codepoints the user passed.
    u32  codepoints_count = user_codepoints_count + 2;
    u32 *codepoints = buffer_push_array(u32, s->memory, codepoints_count);
    codepoints[0] = '\0';
    codepoints[1] = ' ';
    memcpy(&codepoints[2], user_codepoints, user_codepoints_count*sizeof(u32));

    s->font_info        = info;
    s->codepoints_count = codepoints_count;
    s->codepoints       = codepoints;
    s->glyphs           = buffer_push_array(Font_Glyph, s->memory, codepoints_count);
    s->glyph_pixels     = buffer_push_array(u32*, s->memory, codepoints_count);

    for_count(u32, i, codepoints_count){
        font__generate_glyph(s, codepoints[i], i);
    }

    font__bake_glyphs_to_atlas(s);

    img_save_tga("atlas.tga", s->atlas_w, s->atlas_h, s->atlas, s->memory);

    // Begin writing the font to memory
    Font *font = buffer_push_type(Font, s->memory);
    font->header.magic   = Font_File_Magic;
    font->header.version = Font_File_Version;
    font->line_gap     = line_gap;
    font->cap_height   = cap_height;
    font->char_height  = char_height;

    void *font_section = (void *)font;

    void *font_info_section = buffer_write_type(s->memory, &info);
    font->font_info_offset = font_info_section - font_section;
    buffer_write(s->memory, font_file_name, strlen(font_file_name));
    buffer_write(s->memory, "\0", 1);

    font->glyphs_count = s->codepoints_count;
    void *font_codepoints_section = buffer_write(s->memory, s->codepoints, sizeof(u32)*font->glyphs_count);
    void *font_glyphs_section     = buffer_write(s->memory, s->glyphs, sizeof(Font_Glyph)*font->glyphs_count);
    font->glyphs_codepoint_offset = font_codepoints_section - font_section;
    font->glyphs_offset = font_glyphs_section - font_section;

    u32 kerning_pairs_count = (codepoints_count-1)*(codepoints_count-1); // NOTE: Ignore the null glyph.
    Font_Kerning_Pair *kerning_pairs = buffer_push_array(Font_Kerning_Pair, s->memory, kerning_pairs_count);
    f32 *kerning_advance = buffer_push_array(f32, s->memory, kerning_pairs_count);

    u32 kerning_index = 0;
    for(u32 a_index = 1; a_index < codepoints_count; a_index++){
        for(u32 b_index = 1; b_index < codepoints_count; b_index++){
            u32 codepoint_a = s->codepoints[a_index];
            u32 codepoint_b = s->codepoints[b_index];

            u32 glyph_index_a = FT_Get_Char_Index(s->face, codepoint_a);
            u32 glyph_index_b = FT_Get_Char_Index(s->face, codepoint_b);

            FT_Vector kerning = {0, 0};
            FT_Get_Kerning(s->face, glyph_index_a, glyph_index_b, FT_KERNING_DEFAULT, &kerning);

            assert(kerning_index < kerning_pairs_count);
            kerning_pairs[kerning_index]   = (Font_Kerning_Pair){codepoint_a, codepoint_b};
            kerning_advance[kerning_index] = (kerning.x >> 6);
            kerning_index++;
        }
    }

    font->kerning_pairs_count = kerning_pairs_count;
    font->kerning_pairs_offset = ((void *)kerning_pairs) - font_section;
    font->kerning_advance_offset = ((void *)kerning_advance) - font_section;

    font->img_width  = s->atlas_w;
    font->img_height = s->atlas_h;
    void *font_pixels_section = buffer_write(s->memory, s->atlas, sizeof(u32)*s->atlas_w*s->atlas_h);

    font->img_pixels_offset = font_pixels_section - font_section;

    void *buffer_end = (void *)&s->memory->data[s->memory->used];
    font->expected_size = buffer_end - font_section;
    result = (String){(char *)font_section, font->expected_size};

    if(s->stroker){
        FT_Stroker_Done(s->stroker);
        s->stroker = NULL;
    }
    FT_Done_Face(s->face);

    return result;
}

#endif // FONT_BUILDER
