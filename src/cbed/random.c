/*
Authors:   tspike (github.com/tspike2k)
Copyright: Copyright (c) 2024
License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
*/

#include "random.h"
#include "math.h"

Xorshift32 xs32_seed(u32 seed){
    assert(seed != 0);
    Xorshift32 result = {seed};
    return result;
}

u32 xs32_rand(Xorshift32* s){
    u32 num = s->state;
    num ^= num << 13;
    num ^= num >> 17;
    num ^= num << 5;
    s->state = num;

    return num;
}

// Returns [min, max)
u32 random_u32_between(u32 random_value, u32 min, u32 max){
    assert(min < max);
    assert(max < UINT32_MAX);
    // NOTE: For inclusive max, use the following:
    //n % (max - min + 1) + min;
    u32 result = (random_value % (max - min)) + min;
    return result;
}

// TODO: Better distribution?
bool random_bool(u32 random_value){
    bool result = (bool)random_u32_between(random_value, 0, 2);
    return result;
}

// Returns [min, max]
f32 random_f32_between(u32 random_value, float min, float max){
    assert(min < max);
    f32 result = lerp(min, max, random_percent(random_value)); // NOTE: The idea of lerping between min/max thanks to Handmade Hero.
    return result;
}

// Returns [0.0, 1.0]
f32 random_percent(u32 random_value){
    f32 result = (1.0f/ (f32)UINT32_MAX) * (f32)random_value;
    return result;
}

f32 random_angle(u32 random_value){
    f32 result = random_f32_between(random_value, 0, TAU);
    return result;
}

#if 0

// Returns [-1.0, 1.0]
float randomNormal(Xorshift32* state)
{
    return 2.0f * randomPercent(state) - 1.0f;
}

// NOTE: in radians
float randomAngle(Xorshift32* state)
{
    import math : PI;
    return randomPercent(state)*2.0f*PI;
}

float randomSign(Xorshift32* state){
    // TODO: Determine if this is the best way to generate a sign value!
    enum delim = UINT32_MAX / 2;
    return randomU32(state) >= delim ? 1.0f : -1.0f;
}

void shuffle(T)(auto ref T[] items, Xorshift32* rng)
{
    // NOTE: The "modern method" of the Fisher–Yates shuffle algorithm
    foreach_reverse(i, ref item; items)
    {
        auto swapIndex = randomU32Between(rng, 0, (uint)i+1);
        auto temp = items[i];
        items[i] = items[swapIndex];
        items[swapIndex] = temp;
    }
}

#endif
