/*
Authors:   tspike (github.com/tspike2k)
Copyright: Copyright (c) 2026
License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
*/

/*
This is version 2 of the Font system. This version simplifies the API quite a lot. To load a
font, read the entire .fnt file into memory and then cast the base pointer to a Font pointer.
No additional work is required.
*/

#ifndef FONT_H
#define FONT_H

// TODO: Rename font to Bmp_Font? Baked_Font? Img_Font?

#include "common.h"
#include "math.h"

#define Font_File_Magic (('F' << 0) | ('o' << 8) | ('n' << 16) | ('t' << 24))
#define Font_File_Version 2

typedef struct{
    u32 height;
    u32 stroke;
    u32 fill_color;
    u32 stroke_color;
    u32 reserved[4];
} Font_Info;

typedef struct{
    u32 a;
    u32 b;
} Font_Kerning_Pair;

typedef struct{
    u32 width;
    u32 height;
    u32 advance;
    Vec2 offset;
    Vec2 uv_min;
    Vec2 uv_max;
} Font_Glyph;

typedef struct{
    u32 magic;
    u32 version;
} Font_Header;

typedef struct {
    Font_Header header;
    u32         font_info_offset;
    u32         line_gap;
    u32         cap_height;
    u32         char_height; // NOTE: Maximum character height
    u32         glyphs_count;
    u32 		glyphs_codepoint_offset;
    u32 		glyphs_offset;
    u32         kerning_pairs_count;
    u32 		kerning_pairs_offset;
    u32         kerning_advance_offset;
    u32 		img_width;
    u32 		img_height;
    u32		    img_pixels_offset;
    u32		    expected_size; // Expected size of the file. If we load the file and it's less than this, there was some sort of error.
    u32         reserved[8];
    u32         user_data[8];
} Font;

/*
IMPORTANT! These helper functions expect a properly formed font to be loaded into memory. The
font_get_glyph function expects at least one glyph to be present in the memory of the loaded
font. If the requested glyph isn't found, it will always return the first glyph (typically
set to the Null Glyph). Testing if the font even contains glyphs should be done before
calling these functions, just in case. For example, a draw_text function should exit early
if font->glyphs_count is 0.
*/
Cbed_API bool font_is_valid(Font* font, size_t memory_size);
Cbed_API Font_Glyph* font_get_glyph(Font* font, u32 codepoint);
Cbed_API f32 font_get_kerning_advance(Font* font, u32 prev_codepoint, u32 codepoint);
Cbed_API f32 font_get_text_width(Font* font, String text);

////
//
// Font generation utilities
//
////

#ifdef FONT_BUILDER

typedef struct Font_Builder Font_Builder;

Cbed_API Font_Builder *font_builder_begin(Buffer *buffer);
Cbed_API String font_builder_generate(Font_Builder *builder, Font_Info info, const char* font_file_name, u32 *codepoints, u32 codepoints_count);
Cbed_API void font_builder_end(Font_Builder *builder);

#endif // FONT_BUILDER

#endif // FONT_H
