//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2026
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

/*
This program opens a list of asset files (images, bitmap fonts, etc) and bakes their
contents into byte arrays. These arrays can be compiled into an application. Doing this
gives access to these assets without the need to need to manually read them into memory.
*/

// TODO: Use the file operations from cbed instead?
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "cbed/common.c"

const char *asset_file_paths[] = {
    "./data/font_en.fnt",
};

static bool file_read_into_memory(FILE *f, Buffer *memory){
    bool result = false;

    fseek(f, 0, SEEK_END);
    size_t total_bytes = ftell(f);
    fseek(f, 0, SEEK_SET);

    assert(total_bytes < memory->size - memory->used);
    // TODO: Check that fread works!
    fread(memory->data, total_bytes, 1, f);
    memory->used = total_bytes;

    return true;
}

u8 g_memory[2*1024*1024];

static String strip_filename_from_path(String file_path){
    String result = file_path;

    char prev_c = '\0';
    for_count(size_t, i, file_path.size){
        char *place = &file_path.text[i];
        if(prev_c == '/'){
            result.size -= place - result.text;
            result.text = place;
        }

        prev_c = *place;
    }
    return result;
}

static String get_var_name(String file_path){
    String result = strip_filename_from_path(file_path);
    char *ext = str_find_last(result, '.');
    if(ext){
        result.size = ext - result.text;
    }

    return result;
}

int main(){
    Buffer memory = {g_memory, Array_Len(g_memory)};

    for_count(u32, asset_index, Array_Len(asset_file_paths)){
        Buffer memory_restore = memory;

        String file_path = str(asset_file_paths[asset_index]);
        FILE *f = fopen(file_path.text, "r");
        if(f){
            if(file_read_into_memory(f, &memory)){
                String var_name = get_var_name(file_path);
                printf("static const u8 %.*s[] = {\n", (int)var_name.size, var_name.text);

                for_count(size_t, i, memory.used){
                    printf("0x%x,", memory.data[i]);
                }

                printf("};\n");
            }

            fclose(f);
        }
        else{
            printf("Unable to open file %s: %s\n", file_path, strerror(errno));
        }
        memory = memory_restore;
    }

    return 0;
}
