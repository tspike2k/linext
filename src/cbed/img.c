//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2025
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

#include "img.h"
#include "files.h"
#include <assert.h>

Cbed_API u32 premultiply_alpha(u32 c){
    u8 r = (c) & 0xff;
    u8 g = (c >> 8) & 0xff;
    u8 b = (c >> 16) & 0xff;
    u8 a = (c >> 24) & 0xff;

    float fa = ((float)a / 255.0f);
    float fr = ((float)r / 255.0f) * fa;
    float fg = ((float)g / 255.0f) * fa;
    float fb = ((float)b / 255.0f) * fa;

    u32 result = (u32)(a << 24)
                | (u32)(fb * 255.0f + 0.5f) << 16
                | (u32)(fg * 255.0f + 0.5f) << 8
                | (u32)(fr * 255.0f + 0.5f);
    return result;
}

Cbed_API u32 bgra_to_rgba(u32 c){
    u8 sr = (u8)((c >> 16) & 0xff);
    u8 sg = (u8)((c >>  8) & 0xff);
    u8 sb = (u8)((c >>  0) & 0xff);
    u8 sa = (u8)((c >> 24) & 0xff);

    u32 result = (u32)(sr <<  0) | (u32)(sg <<  8)
               | (u32)(sb << 16) | (u32)(sa <<  24);
    return result;
}

Cbed_API u32 rgba_to_bgra(u32 c){
    u8 sr = (u8)((c >>  0) & 0xff);
    u8 sg = (u8)((c >>  8) & 0xff);
    u8 sb = (u8)((c >> 16) & 0xff);
    u8 sa = (u8)((c >> 24) & 0xff);

    u32 result = (u32)(sr << 16) | (u32)(sg <<  8)
               | (u32)(sb <<  0) | (u32)(sa <<  24);
    return result;
}

Cbed_API u32 color_from_v4(Vec4 v){
    u32 cr = (u32)(v.r * 255.0f);
    u32 cg = (u32)(v.g * 255.0f);
    u32 cb = (u32)(v.b * 255.0f);
    u32 ca = (u32)(v.a * 255.0f);

    u32 result = (cr << 0) | (cg << 8) | (cb << 16) | (ca << 24);
    return result;
}

Cbed_API Vec4 v4_from_color(u32 c){
    u8 r = (u8)((c >>  0) & 0xff);
    u8 g = (u8)((c >>  8) & 0xff);
    u8 b = (u8)((c >> 16) & 0xff);
    u8 a = (u8)((c >> 24) & 0xff);

    Vec4 result = v4(
        (f32)r / 255.0f,
        (f32)g / 255.0f,
        (f32)b / 255.0f,
        (f32)a / 255.0f
    );
    return result;
}

////
//
// TGA Files
//
////

/*
TGA loading code based on text from the following source:
https://paulbourke.net/dataformats/tga/
*/

#define TGA_Data_Type_Uncompressed_RGB 2
#define TGA_Desc_Upper_Left_Origin     (1 << 5)

#pragma pack(push, 1)
typedef struct {
    u8 id_length;
    u8 colormap_type;
    u8 data_type;
    s16 colormap_origin;
    s16 colormap_length;
    u8 colormap_depth;
    s16 x_origin;
    s16 y_origin;
    s16 width;  // TODO: Is this really signed?
    s16 height; // TODO: Is this really signed?
    u8 bits_per_pixel;
    u8 image_desc;
} TGA_Header;
#pragma pack(pop)

static bool img__tga_is_non_interleaved(TGA_Header* header){
    bool result = !((header->image_desc) & ((1 << 6) | (1 << 7)));
    return result;
}

Cbed_API Img_Pixels img_load_tga_from_memory(const char *file_name, void* data, size_t data_size, Buffer *dest){
    Img_Pixels result = {0};

    Buffer src = {(u8 *)data, data_size};
    TGA_Header *header = buffer_read_type(TGA_Header, &src);
    if(!header){
        fmt_msg("ERROR: {0} is too short to be a valid TGA file.\n", fmt_cstr(file_name));
        return result;
    }

    // TODO: Handle 24-bit pixel data
    if(header->data_type != TGA_Data_Type_Uncompressed_RGB){
        fmt_msg("ERROR: Unsupported TGA file for {0}. File must be an uncompressed RGB image.\n", fmt_cstr(file_name));
        return result;
    }

    if(header->bits_per_pixel != 32){
        fmt_msg("ERROR: Unsupported TGA file for {0}. File must be a 32-bit image (got {1}-bit).\n", fmt_cstr(file_name), fmt_i(header->bits_per_pixel));
        return result;
    }

    if(!(header->image_desc & TGA_Desc_Upper_Left_Origin)){
        fmt_msg("ERROR: Unsupported TGA file for {0}. Origin must be at the upper-left.\n", fmt_cstr(file_name));
        return result;
    }

    if(!img__tga_is_non_interleaved(header)){
        fmt_msg("ERROR: Unsupported TGA file for {0}. Pixel data must be non-interleaved.\n", fmt_cstr(file_name));
        return result;
    }


    u32 width  = header->width;
    u32 height = header->height;
    u32 *src_pixels = buffer_read_array(u32, &src, width*height);
    if(!src_pixels){
        fmt_msg("ERROR: Unable to read pixel data from TGA file {0}\n", fmt_cstr(file_name));
        return result;
    }

    u32 *out_pixels = buffer_push_array(u32, dest, width*height);
    for(u32 i = 0; i < width*height; i++){
        out_pixels[i] = bgra_to_rgba(src_pixels[i]);
        /*out_pixels[i] = argb_to_rgba(src_pixels[i]);*/
        /*out_pixels[i] = src_pixels[i];*/
    }

    result.width  = width;
    result.height = height;
    result.data   = out_pixels;

    return result;
}

Cbed_API Img_Pixels img_load_tga(const char* file_name, Buffer* dest, Buffer *scratch){
    Buffer scratch_restore = *scratch;

    Img_Pixels result = {0};
    String contents = file_read_into_memory(file_name, scratch);
    if(contents.size){
        result = img_load_tga_from_memory(file_name, contents.text, contents.size, dest);
    }

    *scratch = scratch_restore;
    return result;
}

Cbed_API bool img_save_tga(const char* file_name, u32 width, u32 height, u32 *pixels, Buffer *scratch){
    Buffer scratch_restore = *scratch;

    TGA_Header *header     = buffer_push_type(TGA_Header, scratch);
    header->data_type      = TGA_Data_Type_Uncompressed_RGB;
    header->bits_per_pixel = 32;
    header->width          = (s16)width;
    header->height         = (s16)height;
    header->image_desc     = TGA_Desc_Upper_Left_Origin;

    u32 *out_pixels = buffer_push_array(u32, scratch, width*height);
    for(u32 i = 0; i < width*height; i++){
        out_pixels[i] = rgba_to_bgra(pixels[i]);
    }

    bool result = true; // TODO: We should really have a way to tell if a write succeeds or not.
    file_write_from_memory(file_name, header, scratch->used - scratch_restore.used);
    *scratch = scratch_restore;
    return result;
}
