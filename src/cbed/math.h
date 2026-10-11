//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2025
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

#ifndef CBED_MATH_H
#define CBED_MATH_H

#include "common.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(a, min, max) ((a) < (min) ? (min) : ((a) > (max) ? (max) : (a)))

#ifdef OS_Linux

#define pow   __builtin_powif
#define ceil  __builtin_ceilf
#define floor __builtin_floorf
#define sqrt  __builtin_sqrt
#define sin   __builtin_sinf
#define cos   __builtin_cosf
#define tan   __builtin_tanf
#define absf  __builtin_fabsf
#define atan2 __builtin_atan2
#define exp   __builtin_exp
#define modf  __builtin_fmodf

#endif

#define PI  3.141592653589793f
#define TAU (PI*2.0f)

typedef struct{
    float x, y;
} Vec2;

typedef union{
    struct{
        float x, y, z;
    };
    struct{
        float r, g, b;
    };
} Vec3;

typedef union{
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
} Vec4;

#define v2(x, y) (Vec2){(f32)(x), (f32)(y)}
#define v3(x, y, z) (Vec3){(f32)(x), (f32)(y), (f32)(z)}
#define v4(x, y, z, w) (Vec4){{(f32)(x), (f32)(y), (f32)(z), (f32)(w)}}

#define v2_xyz(v) (Vec3){v.x, v.y, 0}

Cbed_API Vec2  v2_sub(Vec2 a, Vec2 b);
Cbed_API Vec2  v2_add(Vec2 a, Vec2 b);
Cbed_API Vec2  v2_mul(Vec2 a, Vec2 b);
Cbed_API Vec2  v2_muls(Vec2 a, float b);
Cbed_API Vec2  v2_div(Vec2 a, Vec2 b);
Cbed_API Vec2  v2_lerp(Vec2 a, Vec2 b, float t);
Cbed_API Vec3  v3_sub(Vec3 a, Vec3 b);
Cbed_API Vec3  v3_add(Vec3 a, Vec3 b);
Cbed_API Vec3  v3_mul(Vec3 a, Vec3 b);
Cbed_API Vec3  v3_muls(Vec3 a, float b);
Cbed_API Vec3  v3_div(Vec3 a, Vec3 b);
Cbed_API Vec2  v2_normalize(Vec2 v);
Cbed_API Vec3  v3_normalize(Vec3 a);
Cbed_API float v2_squared(Vec2 v);
Cbed_API float v3_squared(Vec3 v);
Cbed_API float v2_length(Vec2 v);
Cbed_API float v3_length(Vec3 v);
Cbed_API float v2_dot(Vec2 a, Vec2 b);
Cbed_API float v3_dot(Vec3 a, Vec3 b);
Cbed_API Vec3  cross(Vec3 a, Vec3 b);
Cbed_API Vec4  v4_lerp(Vec4 a, Vec4 b, f32 t);

Cbed_API float squared(float n);
Cbed_API float v2_len_sq(Vec2 a);
Cbed_API float v2_dist_sq(Vec2 a, Vec2 b);
Cbed_API float v3_dist_sq(Vec3 a, Vec3 b);

typedef struct{
    Vec2 center;
    Vec2 extents;
} Rect;

Cbed_API bool circles_overlap(Vec2 a_center, f32 a_radius, Vec2 b_center, f32 b_radius);

Cbed_API Rect rect_from_min_wh(Vec2 p, float w, float h);
Cbed_API Rect rect_from_min_max(Vec2 min_p, Vec2 max_p);
Cbed_API Vec2 rect_min(Rect r);
Cbed_API Vec2 rect_max(Rect r);
Cbed_API f32  rect_left(Rect r);
Cbed_API f32  rect_right(Rect r);
Cbed_API f32  rect_top(Rect r);
Cbed_API f32  rect_bottom(Rect r);
Cbed_API Vec2 rect_top_left(Rect r);
Cbed_API f32  rect_width(Rect r);
Cbed_API f32  rect_height(Rect r);
Cbed_API bool is_point_inside_rect(Vec2 p, Rect r);
Cbed_API bool rects_overlap(Rect a, Rect b);

Cbed_API Rect rect_shrink(Rect a, Vec2 size);
Cbed_API Rect rect_expand(Rect a, Vec2 size);
// rect_cut_X concept thanks to Jonathan Blow
Rect rect_cut_left(Rect r, f32 size);
Rect rect_cut_right(Rect r, f32 size);
Rect rect_cut_top(Rect r, f32 size);
Rect rect_cut_bottom(Rect r, f32 size);

typedef struct{
    float m[4][4];
} Mat4;

Cbed_API Mat4 mat4_mul(Mat4 a, Mat4 b);
Cbed_API Mat4 mat4_transpose(Mat4 a);
Cbed_API Mat4 mat4_translate(Vec3 offset);

#define Mat4_Identity ((Mat4){{ \
    {1, 0, 0, 0},               \
    {0, 1, 0, 0},               \
    {0, 0, 1, 0},               \
    {0, 0, 0, 1},               \
}})

Cbed_API f32 lerp(f32 a, f32 b, f32 t);
Cbed_API u32 premultiply_alpha(u32 c);
Cbed_API u64 round_up_power_of_two(u64 n);
Cbed_API float deg_to_rad(float degrees);
Cbed_API Vec3 polar_to_world(Vec3 polar, Vec3 target_pos);

#ifdef __cplusplus

template<typename T> T min(T a, T b){
    T result = MIN(a, b);
    return result;
}

template<typename T> T max(T a, T b){
    T result = MAX(a, b);
    return result;
}

template<typename T> void clamp(T* a, T min_val, T max_val){
    *a = CLAMP(*a, min_val, max_val);
}

Vec2 operator+(Vec2 l, Vec2 r);
Vec2 operator-(Vec2 l, Vec2 r);
void operator+=(Vec2 &l, Vec2 r);
void operator-=(Vec2 &l, Vec2 r);
Vec2 operator*(Vec2 l, f32 r);
Vec2 operator-(Vec2 l);
Vec2 operator*(Vec2 l, Vec2 r);
Vec2 operator/(Vec2 l, f32 r);

Vec3 operator*(Vec3 l, f32 r);
Vec3 operator*(f32 l, Vec3 r);
Vec3 operator*(Vec3 l, Vec3 r);
Vec3 operator+(Vec3 l, Vec3 r);
Vec3 operator-(Vec3 l, Vec3 r);
void operator+=(Vec3 &l, Vec3 r);
void operator-=(Vec3 &l, Vec3 r);
Vec3 operator-(Vec3 l);

#endif // __cplusplus

#endif // CBED_MATH_HPP
