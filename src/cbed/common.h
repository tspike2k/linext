//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2025
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

/*
To build this library as a shared object, one must define Cbed_API differently for the shared
library and the binary that links to it. Here's an example script for future reference:
    gcc $FLAGS -nostdlib -shared ./src/cbed.c -o ./build/cbed.so
    g++ $FLAGS -DCbed_API='extern "C"' ./src/app.cpp ./build/cbed.so -o ./build/app $LIBS
*/

#ifndef CBED_COMMON_H
#define CBED_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // size_t

//
// Common types
//
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

//
// OS compile-time constants
//
#ifdef __gnu_linux__
#define OS_Linux
#endif

//
// Useful preprocessor defines
//

#ifdef __cplusplus
  #define Cbed_C_Lib extern "C"
#else
  #define Cbed_C_Lib extern
#endif

#ifdef OS_Linux
#  define Cbed_Inline inline __attribute__((always_inline))
#else
#  define Cbed_Inline inline
#endif

#ifndef Cbed_API
#  ifdef OS_Linux
#    define Cbed_API __attribute__((visibility("default")))
#  else
#    define Cbed_API
#  endif
#endif

//
// Misc macros
//

#define for_u32(i, max) for(u32 i = 0; i < (max); i++)
#define for_count(T, i, max) for(T i = 0; i < (max); i++)
#define for_range(T, i, min, max) for(T i = (min); i < (max); i++)
#define Macro_Join2(a, b) a##b
#define Macro_Join(a, b) Macro_Join2(a, b)

#define Min(a, b) (a) < (b) ? (a) : (b)
#define Max(a, b) (a) > (b) ? (a) : (b)

#define Clear_Flags(bits, flags) ((bits) & ~(flags))

//
// Strings
//

typedef struct{
    char*  text;
    size_t size;
} String;

Cbed_API String str(const char* s);
Cbed_API bool char_is_whitespace(char c);
Cbed_API void str_advance(String *reader);
Cbed_API String str_eat_line(String *reader);
Cbed_API void str_skip_whitespace(String *reader);
Cbed_API bool str_match(String a, String);
Cbed_API char *str_find_last(String s, char c);
Cbed_API bool  str_ends_with(String s, String end);

//
// Buffers
//

typedef struct {
    u8*    data;
    size_t size;
    size_t used;
} Buffer;

#define buffer_push_array(T, buffer, count) (T*)buffer_push_bytes(buffer, sizeof(T)*(count))
#define buffer_push_type(T, buffer) (T*)buffer_push_bytes(buffer, sizeof(T))
Cbed_API void *buffer_push_bytes(Buffer *buffer, size_t bytes);

// TODO: Reads and writes can fail if the buffer is too small. Right now we're gaurding against
// that by using asserts, but this doesn't address the issue past debug builds. Perhaps we
// should pass a boolean flag to indicate success or failure?
#define buffer_write_type(buffer, t) buffer_write(buffer, t, sizeof(*t))
Cbed_API void *buffer_write(Buffer *buffer, const void* data, size_t data_size);

#define buffer_read_type(T, buffer) (T*)buffer_read(buffer, sizeof(T))
#define buffer_read_array(T, buffer, count) (T*)buffer_read(buffer, sizeof(T)*(count))
Cbed_API void *buffer_read(Buffer *buffer, size_t bytes);

// NOTE: buffer_begin_writer reserves all the remaining space in the source buffer and returns
// it in a secondary buffer. Calling buffer_end_writer will return how much of this space has
// been used since the initial call to buffer_begin_writer. This can be used to generate strings
// or even write binary blobs. If generating strings, passing true for the null_terminate
// argument will ensure the result is null terminated, even if the result must be truncated.
//
// Example:
//
//   Buffer writer = buffer_begin_writer(&buffer);
//   buffer_put_string(str_lit("Hello, "));
//   buffer_put_string(str_lit("World!"));
//   String s = buffer_end_writer(&buffer, writer, true);
//   printf("%s\n", s.text);
Cbed_API Buffer buffer_begin_writer(Buffer *buffer);
Cbed_API String buffer_end_writer(Buffer *buffer, Buffer writer, bool null_terminate);

// NOTE: The buffer_put_text function truncates the result if no more roo exists in the buffer.
// When concatonating strings, calling buffer_null_terminate will ensure the text is null
// terminated, even if the string had been truncated.
Cbed_API char *buffer_put_text(Buffer *buffer, const char *text, size_t text_len);
Cbed_API char *buffer_null_terminate(Buffer* buffer);

// Conversion functions
Cbed_API bool str_to_f32(const char *s, size_t s_len, f32* f);

#define str_lit(s) (String){(char *)(s), Array_Len((s))-1} // sizeof includes the null terminator when used on string literals. Interesting.
#define make_str(s, count) (String){(char*)(s), (count)}

//
// String formatting functions
//

//String int_to_string(s64 n, u32 base, Buffer* buffer);
Cbed_API String uint_to_string(u64 n, u32 base, char* buffer, size_t buffer_size);
Cbed_API String float_to_string(f32 f, u32 precision, char *buffer, size_t buffer_size);

typedef struct{
    uint32_t info; // TODO: This should contain both the arg type and an array count. Array count is limited!
    union{
        int64_t     data_int;
        uint64_t    data_uint;
        float       data_float;
        const char *data_cstr;
        void       *data_pointer;
    };
} Fmt_Arg;

// NOTE: The macros fmt_buffer and fmt_out both expand to a scoped code block. As such,
// fmt_buffer can't return a value like a normal function. But the caller needs to know
// how much text was written into the buffer. To simplify that issue, we provide the
// Fmt_Buffer data type which is passed by pointer to the fmt_buffer macro.

// NOTE: This callback is used internally by the fmt_msg_* functions to copy text to some sort
// of destination. The default destination is buffered output to stdout. This can be changed
// using the fmt_msg_set_dest function. If the function is called passing NULL as the text
// parameter this indicates a flush request. If the destination is some form of buffered ouput
// this case should be handled.
typedef void (*Fmt_Put_Func)(const char *text, size_t text_count, void *user_data);

#define Array_Len(a) (sizeof((a)) / sizeof((a)[0]))
#define Offset_Of(type, member) (size_t)&(((type*)0)->member)

// NOTE: If zero arguments are passed as varargs then fmt_args will still have a length of 1.
// This shouldn't cause much of an issue as that bug can be pretty easily found, but it is a
// limitation of this approach.
#define fmt_msg(s, ...) \
    fmt_msg_raw((s), (Fmt_Arg []){__VA_ARGS__}, Array_Len( ((Fmt_Arg []){__VA_ARGS__}) ))

#define fmt_buffer(s, dest, ...) \
    fmt_buffer_raw((s), dest, (Fmt_Arg []){__VA_ARGS__}, Array_Len( ((Fmt_Arg []){__VA_ARGS__}) ))

Cbed_API Fmt_Arg fmt_i(int64_t value);
Cbed_API Fmt_Arg fmt_cstr(const char *s);
Cbed_API Fmt_Arg fmt_f(float value);

Cbed_API void fmt_msg_set_dest(Fmt_Put_Func put, void *user_data);
Cbed_API void fmt_msg_put(const char* msg, size_t msg_length);
Cbed_API void fmt_msg_puts(const char* msg);
Cbed_API void fmt_msg_raw(const char *fmt_string, Fmt_Arg *args, size_t args_count);

Cbed_API String fmt_buffer_raw(const char *fmt_string, Buffer *buffer, Fmt_Arg *args, size_t args_count);

// NOTE: The following is typically used internally but use can make it easier to write type-safe
// wrappers in other languages.
typedef struct{
    String reader;
    bool   done;
    bool   is_spec;
    u32    arg_index;
} Fmt_Parser;

Cbed_API Fmt_Parser fmt_parse(const char *fmt_string, size_t fmt_string_len);
Cbed_API String fmt_parse_next(Fmt_Parser *parser);

#endif // CBED_COMMON_H
