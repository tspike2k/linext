//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2025
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

#ifndef CBED_FILES_H
#define CBED_FILES_H

#include "common.h"

enum{
    File_Flag_Is_Open        = (1 << 0),
    File_Flag_Read           = (1 << 1),
    File_Flag_Write          = (1 << 2),
    File_Flag_No_Trunc       = (1 << 3),
    File_Flag_No_Open_Errors = (1 << 5),
    File_Flag_Error          = (1 << 6),
};

typedef enum{
    File_Type_None,
    File_Type_Unknown,
    File_Type_Directory,
    File_Type_File,
} File_Type;

typedef struct{
    u32    flags;
    u64    handle;
    size_t cursor; // TODO: Is this for file streaming? Is this a good idea?
} File;

// NOTE: Read/write operations are done in a blocking mode.
Cbed_API bool file_open(File *file, const char *file_path, uint32_t flags);
Cbed_API void file_close(File *file);
Cbed_API size_t file_read(File *file, size_t offset, void *buffer, size_t buffer_size);
Cbed_API void file_write(File *file, size_t offset, void *buffer, size_t buffer_size);
Cbed_API size_t file_get_size(File* file);
Cbed_API void file_delete(const char *file_path);
Cbed_API size_t file_stream_in(File *file, void *buffer, size_t buffer_size);
Cbed_API void file_stream_out(File *file, void *buffer, size_t buffer_size);
Cbed_API bool file_exists(const char *file_path);

Cbed_API void file_write_from_memory(const char *file_name, void *data, size_t size);
Cbed_API String file_read_into_memory(const char *file_name, Buffer *buffer);

Cbed_API File file_get_stdin();
Cbed_API File file_get_stdout();
Cbed_API File file_get_stderr();

Cbed_API const char *get_executable_path(Buffer *buffer);

typedef struct{
    u32 flags;
    u8  internal[16];
} File_Lib;

Cbed_API bool file_open_lib(File_Lib *lib, const char *file_name);
Cbed_API void    *file_load_symbol_raw(File_Lib *lib, const char *symbol);
Cbed_API void     file_close_lib(File_Lib *lib);
#define File_Load_Symbol(lib, sym) sym = (Macro_Join(sym, _func))file_load_symbol_raw(lib, #sym)

typedef struct{
    File_Type   file_type;
    const char *file_name;
    u8          internal[64];
} File_Walker;

Cbed_API void file_walker_begin(File_Walker *walker, const char *dir_path);
Cbed_API void file_walker_end(File_Walker *walker);
Cbed_API bool file_walker_advance(File_Walker *walker);
Cbed_API void file_walker_enter_directory(File_Walker *walker);
Cbed_API String file_walker_make_path(File_Walker *walker, Buffer *buffer);

typedef struct{
    u8 internal[64];
} File_Watcher;

enum File_Watcher_Event_Type{
    File_Watcher_Event_None,
    File_Watcher_Event_Create,
    File_Watcher_Event_Delete,
    File_Watcher_Event_Modify,
};

typedef struct{
    u32         type;
    u32         watch_id;
    const char *name;
} File_Watcher_Event;

#define File_Watcher_Bad_ID UINT32_MAX

Cbed_API void file_watcher_begin(File_Watcher *watcher, void *buffer, u32 buffer_size);
Cbed_API void file_watcher_end(File_Watcher *watcher);
Cbed_API u32  file_watcher_add(File_Watcher *watcher, const char *file_path);
Cbed_API void file_watcher_update(File_Watcher *watcher);
Cbed_API bool file_watcher_next_event(File_Watcher *watcher, File_Watcher_Event *evt);

typedef struct File_Pather File_Pather;

Cbed_API File_Pather *file_path_begin(String base_path, Buffer *temp);
Cbed_API void file_path_add(File_Pather *pather, String entry_name);
Cbed_API String file_path_end(File_Pather *pather, Buffer *dest);


#endif // CBED_FILES_H
