/*
Authors:   tspike (github.com/tspike2k)
Copyright: Copyright (c) 2026
License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
*/

/*
Many of the functions in here take a psuedo-randomly generated u32 and shape it in some way.
This is a little more clunky than it could be, but it greatly reduces the number of
functions we'd need to write while keeping the shaping functions easy enough to call.

This does mean we would need to call two function in order to get both a random value and
put it into the right range.
*/

#ifndef CBED_RANDOM_H
#define CBED_RANDOM_H

#include "common.h"

typedef struct{
    u32 state;
} Xorshift32;

Xorshift32 xs32_seed(u32 seed);
u32 xs32_rand(Xorshift32 *rng);

u32   random_u32_between(u32 random_value, u32 min, u32 max); // Returns [min, max)
bool  random_bool(u32 random_value);
f32   random_f32_between(u32 random_value, float min, float max); // Returns [min, max]
f32   random_percent(u32 random_value); // Returns [0.0, 1.0]
f32   random_angle(u32 random_value);

#endif // CBED_RANDOM_H
