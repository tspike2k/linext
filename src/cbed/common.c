//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2025
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

#include "common.h"

#include <string.h> // memcpy
#include <assert.h>

#ifdef OS_Linux
// TODO: Is there a pow function for integers?
#  define fmt__pow __builtin_powif
#endif // OS_Linux

static const char *fmt__int_table = "0123456789abcdefxp";

static void fmt__default(const char *text, size_t text_count, void *dest);

static char         fmt__memory[1024];
static Buffer       fmt__buffer = {(u8*)fmt__memory, Array_Len(fmt__memory)};
static void        *fmt__msg_dest = &fmt__buffer;
static Fmt_Put_Func fmt__msg_put = fmt__default;

enum{
    Fmt_Type_None,
    Fmt_Type_Unsigned_Integer,
    Fmt_Type_Signed_Integer,
    Fmt_Type_C_String,
    Fmt_Type_Float,
};

Cbed_API Fmt_Arg fmt_i(int64_t value){
    Fmt_Arg result = {0};
    result.info = Fmt_Type_Signed_Integer;
    result.data_int = value;
    return result;
}

Cbed_API Fmt_Arg fmt_f(float value){
    Fmt_Arg result = {0};
    result.info = Fmt_Type_Float;
    result.data_float = value;
    return result;
}

Cbed_API Fmt_Arg fmt_cstr(const char *value){
    Fmt_Arg result = {0};
    result.info = Fmt_Type_C_String;
    result.data_cstr = value;
    return result;
}

#include <stdio.h> // TODO: Use float conversion functions sourced from stb.

Cbed_API String uint_to_string(u64 n, u32 base, char* buffer, size_t buffer_size){
    assert(base <= 16);

    buffer[buffer_size-1] = '0';
    String result = {&buffer[buffer_size-1], 1};

    for(size_t i = buffer_size; i > 0 && n != 0; i--){
        char c = fmt__int_table[n % base];
        buffer[i-1] = c;
        result = (String){&buffer[i-1], buffer_size - i + 1};
        n /= base;
    }
    return result;
}

Cbed_API String float_to_string(f32 f, u32 precision, char *buffer, size_t buffer_size){
    assert(buffer_size >= 512);

    // TODO: Use stb_printf for float formatting
    snprintf(buffer, buffer_size, "%f", f);
    String result = {buffer, strlen(buffer)};

    // Auto-precision. Snip off trailing zeroes.
    if(precision == 0 && result.size > 0){
        size_t cursor = result.size-1;
        while(cursor > 0){
            char c    = result.text[cursor];
            char peek = result.text[cursor-1];
            if(peek == '.' || c != '0'){
                break;
            }
            cursor--;
        }
        result.size = cursor+1;
    }

    return result;
}

static String fmt__s64(char *buffer, size_t buffer_length, int64_t n, uint32_t base){
    assert(base <= 16);

    buffer[buffer_length-1] = '0';
    String result = {&buffer[buffer_length-1], 1};

    for(size_t i = buffer_length; i > 0 && n != 0; i--){
        char c = fmt__int_table[n % base];
        buffer[i-1] = c;
        result = (String){&buffer[i-1], buffer_length - i + 1};
        n /= base;
    }
    return result;
}

static void fmt__arg(Fmt_Arg arg, Fmt_Put_Func put, void *dest){
    char temp_buffer[512];
    size_t buffer_size = Array_Len(temp_buffer);
    String text = {0};

    switch(arg.info){
        default: assert(0); break;

        case Fmt_Type_Signed_Integer:{
            text = fmt__s64(&temp_buffer[0], buffer_size, arg.data_int, 10);
        } break;

        case Fmt_Type_Float:{
            text = float_to_string(arg.data_float, 0, &temp_buffer[0], buffer_size);
        } break;

        case Fmt_Type_C_String:{
            const char* s = arg.data_cstr;
            text = (String){(char*)s, !s ? 0 : strlen(s)};
        } break;
    }

    put(text.text, text.size, dest);
}

Cbed_API size_t buffer_frame_begin(Buffer *buffer){
    size_t result = buffer->used;
    return result;
}

Cbed_API void buffer_frame_end(Buffer* buffer, size_t marker){
    buffer->used = marker;
}

Cbed_API void *buffer_push_bytes(Buffer *buffer, size_t bytes){
    void *result = NULL;
    if(bytes > 0){
        assert(buffer->used + bytes <= buffer->size);
        result = &buffer->data[buffer->used];
        memset(result, 0, bytes);
        buffer->used += bytes;
    }
    return result;
}

Cbed_API void *buffer_write(Buffer *buffer, const void* data, size_t data_size){
    void *result = &buffer->data[buffer->used];
    if(data_size > 0){
        assert(buffer->used + data_size <= buffer->size);
        memcpy(result, data, data_size);
        buffer->used += data_size;
    }
    return result;
}

Cbed_API void *buffer_read(Buffer *buffer, size_t bytes){
    void *result = NULL;
    if(buffer->used + bytes <= buffer->size){
        result = &buffer->data[buffer->used];
        buffer->used += bytes;
    }
    return result;
}

Cbed_API Buffer buffer_begin_writer(Buffer *buffer){
    Buffer result = {&buffer->data[buffer->used], buffer->size - buffer->used};
    buffer->used = buffer->size;
    return result;
}

Cbed_API String buffer_end_writer(Buffer *buffer, Buffer writer, bool null_terminate){
    assert(buffer->size);
    assert(writer.size);

    buffer->used = &writer.data[writer.used] - buffer->data;

    String result = {(char*)writer.data, writer.used};
    if(null_terminate){
        if(buffer->used < buffer->size){
            buffer->data[buffer->used] = 0;
            buffer->used++;
        }
        else{
            // We have no more room left, so truncate the result to null-terminate.
            buffer->data[buffer->used-1] = 0;
            result.size--;
        }
    }
    else{
        assert(buffer->used < buffer->size);
    }

    return result;
}

Cbed_API char *buffer_put_text(Buffer *buffer, const char* text, size_t text_len){
    char *result = (char *)&buffer->data[buffer->used];
    if(text_len){
        size_t availible = buffer->size - buffer->used;
        size_t to_copy = text_len < availible ? text_len : availible;
        memcpy(&buffer->data[buffer->used], text, to_copy);
        buffer->used += to_copy;
    }
    return result;
}

Cbed_API char *buffer_null_terminate(Buffer* buffer){
    char *result = (char *)&buffer->data[buffer->used];
    if(buffer->used < buffer->size){
        buffer->data[buffer->used] = 0;
        buffer->used++;
    }
    else{
        buffer->data[buffer->used-1] = 0;
    }
    return result;
}

//
// Strings
//

Cbed_API String str(const char* s){
    String result = {(char*)s, strlen(s)};
    return result;
}

Cbed_API bool char_is_whitespace(char c){
    bool result = c == ' ' || (c >= '\t' && c <= '\r');
    return result;
}

Cbed_API void str_advance(String* reader){
    assert(reader->size);
    reader->text++;
    reader->size--;
}

Cbed_API String str_eat_line(String *reader){
    // TODO: This is buggy! The reader doesn't skip the ending newline.
    String result = *reader;
    while(reader->size){
        if(reader->text[0] == '\n'){
            result.size = reader->text - result.text;
            str_advance(reader);
            break;
        }
        else if(reader->size > 1 && reader->text[0] == '\r' && reader->text[1] == '\n'){
            result.size = reader->text - result.text;
            str_advance(reader);
            break;
        }
        str_advance(reader);
    }
    return result;
}

Cbed_API void str_skip_whitespace(String *reader){
    while(reader->size && char_is_whitespace(reader->text[0])){
        str_advance(reader);
    }
}

Cbed_API bool str_match(String a, String b){
    bool result = a.size == b.size;
    if(a.size == b.size){
        for(size_t i = 0; i < a.size; i++){
            if(a.text[i] != b.text[i]){
                result = false;
                break;
            }
        }
    }
    return result;
}

Cbed_API bool str_ends_with(String s, String end){
    assert(end.size);

    bool result = true;
    if(s.size >= end.size){
        size_t start_index = s.size + 1 - end.size;
        assert(start_index <= s.size);

        const char *a = &s.text[start_index];
        const char *b = &end.text[0];
        for(size_t i = 0; i < end.size; i++){
            if(*a != *b){
                result = false;
                break;
            }
            a++;
            b++;
        }
    }
    else{
        result = false;
    }

    return result;
}

Cbed_API char *str_find_last(String s, char c){
    char *result = NULL;
    while(s.size > 0){
        if(*s.text == c){
            result = s.text;
        }
        s.text++;
        s.size--;
    }
    return result;
}

////
//
// Conversions
//
////

// This function is a slightly edited version of the stb__clex_parse_float function
// from stb_c_lexer.h by Sean Barrett, et al. The source license used by that library
// is Public Domain, so this adaptation can be considered to be either in the public domain
// or the same license used by the rest of the code in this file.
//
// TODO: Find out what algorithm this is and what it's limitations are.
Cbed_API bool str_to_f64(const char *str, size_t s_len, f64* d){
    const char *p = str;
    double value=0;
    int base=10;
    int exponent=0;

    bool success = true;

    for(;;){
        if (*p >= '0' && *p <= '9')
            value = value*base + (*p++ - '0');
        else{
            success = false;
            break;
        }
    }

    if(*p == '.'){
        double power, addend = 0;
        ++p;
        for (power=1; ; power*=base) {
            if (*p >= '0' && *p <= '9')
                addend = addend*base + (*p++ - '0');
            else{
                success = false;
                break;
            }
        }
        value += addend / power;
    }
    exponent = (*p == 'e' || *p == 'E');

    if (exponent) {
        int sign = p[1] == '-';
        unsigned int exponent=0;
        double power=1;
        ++p;
        if (*p == '-' || *p == '+')
            ++p;
        while (*p >= '0' && *p <= '9')
            exponent = exponent*10 + (*p++ - '0');
        power = fmt__pow(10, exponent); // TODO: Use a builtin pow function? Perhaps we can manage that.
        if (sign)
            value /= power;
        else
            value *= power;
    }

    *d = value;
    return success;
}

Cbed_API bool str_to_f32(const char *s, size_t s_len, f32* f){
    // TODO: It seems there are a wealth of new ideas on how to
    // https://research.swtch.com/fp
    f64 d = 0;
    bool result = str_to_f64(s, s_len, &d);
    *f = d;
    return result;
}

#ifdef OS_Linux

#include <unistd.h>

static void fmt__default_flush(){
    if(fmt__buffer.used > 0){
        write(1, &fmt__buffer.data[0], fmt__buffer.used);
        fmt__buffer.used = 0;
    }
}

#endif // OS_Linux

static void fmt__default(const char *text, size_t text_count, void *dest){
    if(text){
        while(text_count > 0){
            size_t availible = fmt__buffer.size - fmt__buffer.used;
            size_t to_write  = text_count < availible ? text_count : availible;

            memcpy(&fmt__buffer.data[fmt__buffer.used], text, to_write);
            fmt__buffer.used += to_write;
            if(fmt__buffer.used == fmt__buffer.size || text[text_count-1] == '\n'){
                fmt__default_flush();
            }

            text_count -= to_write;
            text += to_write;
        }
    }
    else{
        fmt__default_flush();
    }
}

Cbed_API void fmt_msg_set_dest(Fmt_Put_Func put, void *user_data){
    fmt__msg_dest = user_data;
    fmt__msg_put = put;
}

Cbed_API void fmt_msg_put(const char* msg, size_t msg_length){
    fmt__msg_put(msg, msg_length, fmt__msg_dest);
}

Cbed_API void fmt_msg_puts(const char* msg){
    fmt__msg_put(msg, strlen(msg), fmt__msg_dest);
}

static void fmt__buffer_put(const char* text, size_t text_count, void *dest){
    buffer_write((Buffer*)dest, text, text_count);
}

static void fmt__advance(Fmt_Parser *parser){
    assert(parser->reader.size > 0);
    parser->reader.text++;
    parser->reader.size--;
}

static bool fmt__str_to_u32(String text, u32* value){
    bool success = true;

    *value = 0;
    for(size_t i = text.size; i > 0; i--){
        char c = text.text[i-1];
        if(c >= '0' && c <= '9'){
            *value += (c - '0') * fmt__pow(10, text.size-i);
        }
        else{
            success = false;
            break;
        }
    }

    return success;
}

Fmt_Parser fmt_parse(const char *fmt_string, size_t fmt_string_len){
    Fmt_Parser parser = {0};
    parser.reader = (String){(char*)fmt_string, fmt_string_len};
    return parser;
}

static bool fmt__parse_spec(Fmt_Parser *parser, String s){
    // TODO: For now, we expect the spec text to only contain a single number.
    // In the futre, if we want to support hex or other format options,
    // this will need to become more complex.
    bool success = fmt__str_to_u32(s, &parser->arg_index);
    return success;
}

String fmt_parse_next(Fmt_Parser *parser){
    parser->is_spec = false;

    String result = parser->reader;
    while(parser->reader.size > 0){
        char c = parser->reader.text[0];
        if(c == '{'){
            if(parser->reader.size > 1 && parser->reader.text[1] != '{'){
                result.size = parser->reader.text - result.text;

                fmt__advance(parser);
                String spec_text = parser->reader;
                while(parser->reader.size > 0){
                    char head = parser->reader.text[0];
                    if(head == '}'){
                        spec_text.size = parser->reader.text - spec_text.text;
                        fmt__advance(parser);
                        break;
                    }
                    fmt__advance(parser);
                }

                parser->is_spec = fmt__parse_spec(parser, spec_text);
            }
            else{
                fmt__advance(parser);
                result.size = parser->reader.text - result.text;
                fmt__advance(parser);
            }

            break;
        }
        fmt__advance(parser);
    }

    parser->done = parser->reader.size == 0;
    return result;
}

Cbed_API String fmt_buffer_raw(const char *fmt_string, Buffer *dest, Fmt_Arg* args, size_t args_count){
    Fmt_Parser parser = fmt_parse(fmt_string, strlen(fmt_string));
    String result = {(char*)&dest->data[dest->used], 0};
    while(!parser.done){
        String text = fmt_parse_next(&parser);
        if(text.size){
            fmt__buffer_put(&text.text[0], text.size, dest);
        }
        if(parser.is_spec && parser.arg_index < args_count){
            fmt__arg(args[parser.arg_index], fmt__buffer_put, dest);
        }
    }
    // Null terminate the end of the resulting string.
    result.size = &dest->data[dest->used] - (u8*)result.text;
    size_t term_index = dest->used < dest->size ? dest->used : dest->size;
    dest->data[term_index] = '\0';
    return result;
}

Cbed_API void fmt_msg_raw(const char *fmt_string, Fmt_Arg *args, size_t args_count){
    Fmt_Parser parser = fmt_parse(fmt_string, strlen(fmt_string));
    while(!parser.done){
        String text = fmt_parse_next(&parser);
        if(text.size){
            fmt__msg_put(&text.text[0], text.size, fmt__msg_dest);
        }
        if(parser.is_spec && parser.arg_index < args_count){
            fmt__arg(args[parser.arg_index], fmt__msg_put, fmt__msg_dest);
        }
    }
}
