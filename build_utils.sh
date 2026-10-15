#!/bin/bash
FLAGS="-g -Isrc -Wall -Wno-unused-variable -Wno-unused-function"

gcc $FLAGS -o ./bin/gen_grass ./src/utils/gen_grass.c -lm
gcc $FLAGS -o ./bin/embed_assets ./src/utils/embed_assets.c
