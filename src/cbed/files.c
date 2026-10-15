//------------------------------------------------------------------------------
// Authors:   tspike (github.com/tspike2k)
// Copyright: Copyright (c) 2025
// License:   Boost Software License 1.0 (https://www.boost.org/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

#include "files.h"
#include "common.h"

//------------------------------------------------------------------------------
// OS_Linux
//------------------------------------------------------------------------------
#ifdef OS_Linux

#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h> // malloc, realloc, free
#include <poll.h>

Cbed_API bool file_open(File *file, const char *file_path, uint32_t flags){
    uint32_t default_file_permissions = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

    int permissions = 0;
    int oflags = 0;
    if((flags & File_Flag_Read) && (flags & File_Flag_Write)){
        oflags = O_RDWR|O_CREAT;
    }
    else if(flags & File_Flag_Read){
        oflags = O_RDONLY;
    }
    else if(flags & File_Flag_Write){
        oflags = O_WRONLY|O_CREAT;
        permissions = default_file_permissions;
    }

    if((flags & File_Flag_Write) && !(flags & File_Flag_No_Trunc)){
        oflags |= O_TRUNC;
    }

    bool result = false;
    int fd = open(file_path, oflags, permissions);
    if(fd != -1){
        result = true;
        file->handle = fd;
        file->flags = flags|File_Flag_Is_Open;
    }
    else if(!(flags & File_Flag_No_Open_Errors)){
        const char *error_msg = strerror(errno);
        fmt_msg("Unable to open {0}: {1}\n", fmt_cstr(file_path), fmt_cstr(error_msg));
    }
    return result;
}

Cbed_API size_t file_read(File *file, size_t offset, void *buffer, size_t buffer_size){
    assert(file->flags & File_Flag_Read);

    int fd = (int)file->handle;
    uint8_t *dest = (uint8_t*)buffer;
    size_t bytes_read = 0;
    while(bytes_read < buffer_size){
        ssize_t r = pread(fd, &dest[bytes_read], buffer_size - bytes_read, offset + bytes_read);
        if(r == -1){
            if(errno == -EINTR){
                // Reading was interupted by a signal. Do nothing to try again.
                continue;
            }
            else{
                const char *error_msg = strerror(errno);
                fmt_msg("Failed to read from file: {0}\n", fmt_cstr(error_msg));
                file->flags |= File_Flag_Error;
            }
        }
        else{
            bytes_read += (size_t)r;
        }
        break;
    }

    file->cursor = offset + bytes_read;
    return bytes_read;
}

Cbed_API void file_write(File *file, size_t offset, void *buffer, size_t buffer_size){
    assert(file->flags & File_Flag_Write);

    int fd = (int)file->handle;
    uint8_t *dest = (uint8_t*)buffer;
    size_t buffer_written = 0;
    while(buffer_written < buffer_size){
        ssize_t r = pwrite(fd, &dest[buffer_written], buffer_size - buffer_written, offset + buffer_written);
        if(r == -1){
            if(errno == -EINTR){
                // Writing was interupted by a signal. Do nothing to try again.
                continue;
            }
            else{
                const char *error_msg = strerror(errno);
                fmt_msg("Failed to write to file: {0}\n", fmt_cstr(error_msg));
                file->flags |= File_Flag_Error;
            }
        }
        else{
            buffer_written += (size_t)r;
        }
        break;
    }

    file->cursor = offset + buffer_written;
}

Cbed_API size_t file_stream_in(File *file, void *buffer, size_t buffer_size){
    assert(file->flags & File_Flag_Read);

    int fd = (int)file->handle;
    uint8_t *dest = (uint8_t*)buffer;
    size_t bytes_read = 0;
    while(bytes_read < buffer_size){
        ssize_t r = read(fd, &dest[bytes_read], buffer_size - bytes_read);
        if(r == -1){
            if(errno == -EINTR){
                // Reading was interupted by a signal. Do nothing to try again.
                // TODO: Log the error.
                // log_error("Failed to read from file: {0}\n", strerror(errno));
                continue;
            }
            else{
                file->flags |= File_Flag_Error;
            }
        }
        else{
            bytes_read += (size_t)r;
        }
        break;
    }

    return bytes_read;
}

Cbed_API void file_stream_out(File *file, void *buffer, size_t buffer_size){
    assert(file->flags & File_Flag_Write);

    int fd = (int)file->handle;
    uint8_t *dest = (uint8_t*)buffer;
    size_t buffer_written = 0;
    while(buffer_written < buffer_size){
        ssize_t r = write(fd, &dest[buffer_written], buffer_size - buffer_written);
        if(r == -1){
            if(errno == -EINTR){
                // Writing was interupted by a signal. Do nothing to try again.
                // TODO: Log the error.
                // log_error("Failed to read from file: {0}\n", strerror(errno));
                continue;
            }
            else{
                file->flags |= File_Flag_Error;
            }
        }
        else{
            buffer_written += (size_t)r;
        }
        break;
    }
}

Cbed_API void file_close(File* file){
    assert(file->flags & File_Flag_Is_Open);
    int fd = (int)file->handle;
    close(fd);
    file->flags = 0;
}

Cbed_API size_t file_get_size(File* file){
    int fd = (int)file->handle;
    struct stat s;
    size_t result = 0;
    if(fstat(fd, &s) == 0){
        result = s.st_size;
    }
    else{
        /*log("Unable to get file size.\n");*/
    }
    return result;
}

Cbed_API void file_write_from_memory(const char *file_name, void *data, size_t size){
    File file;
    if(file_open(&file, file_name, File_Flag_Write)){
        file_write(&file, 0, data, size);
        file_close(&file);
    }
}

Cbed_API String file_read_into_memory(const char *file_name, Buffer *buffer){
    File file;
    String result = {0};
    if(file_open(&file, file_name, File_Flag_Read)){
        size_t size = file_get_size(&file);
        char *contents = (char*)buffer_push_bytes(buffer, size+1);
        file_read(&file, 0, contents, size);
        contents[size] = '\0';
        result = (String){contents, size};
        file_close(&file);
    }
    return result;
}

Cbed_API File file_get_stdin(){
    uint32_t flags = File_Flag_Is_Open|File_Flag_Read;
    File result = {flags, 0};
    return result;
}

Cbed_API File file_get_stdout(){
    uint32_t flags = File_Flag_Is_Open|File_Flag_Write;
    File result = {flags, 1};
    return result;
}

Cbed_API File file_get_stderr(){
    uint32_t flags = File_Flag_Is_Open|File_Flag_Write;
    File result = {flags, 2};
    return result;
}

Cbed_API const char *get_executable_path(Buffer *buffer){
    // TODO: Make sure the pointer is word aligned.
    const char *result = "./";
    ssize_t count = readlink("/proc/self/exe", (char*)buffer->data, buffer->size - buffer->used);
    if(count > 0){
        // Remove the trailing binary name from the result
        result = (char*)buffer->data;
        char *place = str_find_last(make_str(result, (size_t)count), '/');
        assert(place);
        place++;
        *place = '\0';
        buffer->used += place - result + 1; // Include the null terminator
    }
    else{
        fmt_msg("Unable to get executable path. Falling back to relative path.\n");
    }
    return result;
}

// TODO: Shouldn't this be called file_delete?
Cbed_API void delete_file(const char *file_path){
    // TODO: Error handling?
    unlink(file_path);
}

// Due to various oddities with POSIX, the most reliable way to test if a file exists is to simply
// open the file. Both access and stat can give erronous results if the application is being
// run with the SUID bit set. This seems to be a result of an issue called "TOCTOU."
Cbed_API bool file_exists(const char *file_path){
    bool result = false;
    File file;
    if(file_open(&file, file_path, File_Flag_Read)){
        result = true;
        file_close(&file);
    }
    return result;
}

#if 0
typedef struct{
    u32 flags;
    void *lib;
} File__Lib;

static_assert(sizeof(File__Lib) <= sizeof(File_Lib));

Cbed_API bool file_open_lib(File_Lib *lib, const char *file_name){
    File__Lib *s = (File__Lib *)lib;

    void *dl = dlopen(file_name, RTLD_NOW); // TODO: Is lazy a good idea? Probably.
    if(dl){
        s->lib = dl;
        s->flags |= File_Flag_Is_Open;
        return true;
    }
    else{
        fmt_msg("Unable to open shared object {0}: {1}.\n", fmt_cstr(file_name), fmt_cstr(dlerror()));
        // TODO: Error reporting.
        return false;
    }
}

Cbed_API void *file_load_symbol_raw(File_Lib *lib, const char *symbol){
    File__Lib *s = (File__Lib *)lib;
    assert(s->flags & File_Flag_Is_Open);
    void *result = dlsym(s->lib, symbol);
    return result;
}

Cbed_API void file_close_lib(File_Lib *lib){
    File__Lib *s = (File__Lib *)lib;
    assert(s->flags & File_Flag_Is_Open);
    dlclose(s->lib);
    s->lib = NULL;
    s->flags = 0;
}
#endif

typedef struct {
    DIR           *info;
    struct dirent* contents;
} File__Dir;

typedef struct{
    File_Type   file_type;
    const char *file_name;

    u32        dirs_count;
    u32        dirs_used;
    File__Dir *dirs;
    u32        path_count;
    u32        path_used;
    char      *path;
} File__Walker;

static_assert(sizeof(File_Walker) >= sizeof(File__Walker));

static void file__walker_append_path(File__Walker *s, const char *path){
    size_t len = strlen(path);
    assert(len);

    size_t total_len = len + 2; // Includes the null terminator and possible directory seperator

    size_t next_used = s->path_used + total_len;
    if(next_used > s->path_count){
        s->path_count = Max(next_used*2, s->path_count*2);
        s->path = (char *)realloc(s->path, s->path_count);
    }

    if(s->path_used > 0){
        assert(s->path_used > 1);
        assert(s->path[s->path_used-1] == '\0');
        assert(s->path[s->path_used-2] == '/');
        s->path_used--; // Rewind before the null terminator
    }

    memcpy(&s->path[s->path_used], path, len);
    s->path_used += len;
    if(s->path[s->path_used-1] != '/'){
        s->path[s->path_used++] = '/';
    }
    s->path[s->path_used++] = '\0';
}

static void file__walker_pop_path(File__Walker *s){
    assert(s->path_used > 0);
    assert(s->path[s->path_used-1] == '\0');
    assert(s->path[s->path_used-2] == '/');
    s->path_used -= 2;

    char *place = str_find_last(make_str(s->path, s->path_used), '/');
    place++;
    *place = '\0';
    s->path_used = place - s->path + 1;
}

Cbed_API void file_walker_begin(File_Walker *walker, const char *dir_path){
    File__Walker *s = (File__Walker *)walker;
    memset(s, 0, sizeof(File__Walker));

    s->dirs_count = 16;
    s->dirs = (File__Dir *)malloc(s->dirs_count * sizeof(File__Dir));
    s->path_count = 4096;
    s->path = (char*)malloc(s->path_count);

    s->file_type = File_Type_Directory;
    s->file_name = dir_path;
    file_walker_enter_directory(walker);
}

Cbed_API void file_walker_end(File_Walker *walker){
    File__Walker *s = (File__Walker *)walker;
    if(s->dirs) free(s->dirs);
    if(s->path) free(s->path);
}

Cbed_API bool file_walker_advance(File_Walker *walker){
    File__Walker *s = (File__Walker *)walker;

    bool result = false;
    while(s->dirs_used > 0){
        File__Dir *entry = &s->dirs[s->dirs_used-1];
        struct dirent *next = readdir(entry->info);
        if(next){
            const char *name = next->d_name;
            if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

            result = true;
            s->file_name = name;
            switch(next->d_type){
                default: s->file_type = File_Type_Unknown; break;

                case DT_DIR: s->file_type = File_Type_Directory; break;
                case DT_REG: s->file_type = File_Type_File; break;

                case DT_UNKNOWN:{
                    // TODO: We should try and determine the file type using other means. lstat?
                    assert(0);
                } break;
            }

            break;
        }
        else{
            closedir(entry->info);
            s->dirs_used--;
            file__walker_pop_path(s);
        }
    }

    return result;
}

Cbed_API void file_walker_enter_directory(File_Walker *walker){
    File__Walker *s = (File__Walker *)walker;
    if(s->file_type == File_Type_Directory){
        file__walker_append_path(s, s->file_name);

        DIR *dir = opendir(s->path);
        if(dir){
            if(s->dirs_used == s->dirs_count){
                s->dirs_count *= 2;
                s->dirs = (File__Dir *)realloc(s->dirs, s->dirs_count);
            }

            File__Dir *entry = &s->dirs[s->dirs_used++];
            entry->info = dir;
        }
        else{
            fmt_msg("Unable to walk directory {0}: {1}\n", fmt_cstr(s->path), fmt_cstr(strerror(errno)));
        }
    }
    else{
        fmt_msg("Unable to walk {0}: Not a directory.\n", fmt_cstr(s->path));
    }
}

Cbed_API String file_walker_make_path(File_Walker *walker, Buffer *buffer){
    File__Walker *s = (File__Walker *)walker;
    assert(s->path_used > 1);
    assert(s->path[s->path_used-2] == '/');

    Buffer writer = buffer_begin_writer(buffer);
    buffer_put_string(&writer, (String){s->path, s->path_used-1});
    buffer_put_string(&writer, str(s->file_name));
    String result = buffer_end_writer(buffer, writer, true);

    return result;
}

typedef struct{
    int   inotify_fd;
    u32   to_watch_count;
    int   to_watch[8];
    u32   buffer_length;
    u32   buffer_count;
    u8   *buffer;
    u32   buffer_cursor;
} File__Watch;

static_assert(sizeof(File__Watch) >= sizeof(File_Watcher));

Cbed_API void file_watcher_begin(File_Watcher *watcher, void *buffer, u32 buffer_size){
    File__Watch *s = (File__Watch *)watcher;
    memset(s, 0, sizeof(File__Watch));

    s->inotify_fd = inotify_init1(IN_NONBLOCK);
    s->buffer = (u8 *)buffer;
    s->buffer_length = buffer_size;

    if(s->inotify_fd == -1)
        fmt_msg_puts("Failed to create fd when calling inotify_init1.\n");
}

Cbed_API void file_watcher_end(File_Watcher *watcher){
    File__Watch *s = (File__Watch *)watcher;
    if(s->inotify_fd == -1) return;

    for_count(size_t, i,  s->to_watch_count){
        int fd = s->to_watch[i];
        assert(fd != -1);
        inotify_rm_watch(s->inotify_fd, fd);
    }

    close(s->inotify_fd);
    s->inotify_fd = -1;
}

Cbed_API u32 file_watcher_add(File_Watcher *watcher, const char *file_path){
    File__Watch *s = (File__Watch *)watcher;
    if(s->inotify_fd == -1) return File_Watcher_Bad_ID;

    u32 target_events = IN_CREATE|IN_MODIFY|IN_DELETE|IN_DELETE_SELF;
    u32 result;
    int fd = inotify_add_watch(s->inotify_fd, file_path, target_events);
    if(fd != -1){
        s->to_watch[s->to_watch_count++] = fd;
        result = (u32)fd;
    }
    else{
        result = File_Watcher_Bad_ID;
        const char *err_msg = strerror(errno);
        fmt_msg("Failed to add {0} to inotify watch list: {1}\n", fmt_cstr(file_path), fmt_cstr(err_msg));
    }

    return result;
}

Cbed_API void file_watcher_update(File_Watcher *watcher){
    File__Watch *s = (File__Watch *)watcher;
    if(s->inotify_fd == -1) return;

    struct pollfd pollfds;
    pollfds.fd = s->inotify_fd;
    pollfds.events = POLLIN;
    poll(&pollfds, 1, 0); // TODO: Use something other than poll? epoll?

    s->buffer_count  = 0;
    s->buffer_cursor = 0;
    bool can_read = pollfds.revents & POLLIN;
    if(can_read){
        // TODO: The read should be in file_watcher_next_event... why are we doing that here?
        // TODO: Error handling.
        ssize_t bytes_read = read(s->inotify_fd, s->buffer, s->buffer_length);
        if(bytes_read > 0){
            s->buffer_count = bytes_read;
        }
    }
}

Cbed_API bool file_watcher_next_event(File_Watcher *watcher, File_Watcher_Event *evt){
    File__Watch *s = (File__Watch *)watcher;
    // TODO: We're not looping over the buffer, don't we drop events?
    if(s->inotify_fd == -1) return false;
    if(s->buffer_count - s->buffer_cursor < sizeof(struct inotify_event)) return false;

    struct inotify_event *i_evt = (struct inotify_event *)&s->buffer[s->buffer_cursor];
    s->buffer_cursor += sizeof(struct inotify_event);
    if(s->buffer_count - s->buffer_cursor < i_evt->len) return false; // TODO: Short read. Can this happen?
    s->buffer_count += i_evt->len;

    evt->watch_id = i_evt->wd;
    evt->name     = (char*)i_evt->name;
    if(i_evt->mask & IN_CREATE){
        evt->type = File_Watcher_Event_Create;
    }
    else if((i_evt->mask & IN_DELETE) || (i_evt->mask & IN_DELETE_SELF)){
        evt->type = File_Watcher_Event_Delete;
    }
    else if(i_evt->mask & IN_MODIFY){
        evt->type = File_Watcher_Event_Modify;
    }
    else if(i_evt->mask & IN_MODIFY){
        evt->type = File_Watcher_Event_None;
    }

    return true;
}

#endif // OS_Linux
//------------------------------------------------------------------------------
// OS_Linux
//------------------------------------------------------------------------------

typedef struct File__Pather_Entry File__Pather_Entry;
struct File__Pather_Entry{
    File__Pather_Entry *next;
    String name;
};

struct File_Pather{
    Buffer *buffer;
    /*Buffer  buffer_restore;*/
    String  base_path;
    File__Pather_Entry *first;
    File__Pather_Entry *last;
};

Cbed_API File_Pather *file_path_begin(String base_path, Buffer *temp){
    File_Pather *result = buffer_push_type(File_Pather, temp);
    /*result->buffer_restore = *temp;*/
    result->buffer = temp;
    result->base_path = base_path;
    return result;
}

Cbed_API void file_path_add(File_Pather *pather, String entry_name){
    File__Pather_Entry *entry = buffer_push_type(File__Pather_Entry, pather->buffer);
    entry->name = entry_name;

    if(!pather->first)
        pather->first = entry;
    else if(pather->last)
        pather->last->next = entry;

    pather->last = entry;
}

Cbed_API String file_path_end(File_Pather *pather, Buffer *dest){
#ifdef OS_Windows
    char sep_char = '\\';
#else
    char sep_char = '/';
#endif

    Buffer writer = buffer_begin_writer(dest);
    buffer_put_string(&writer, pather->base_path);

    for(File__Pather_Entry *entry = pather->first; entry; entry = entry->next){
        if(writer.data[writer.used-1] != sep_char){
            buffer_put_string(&writer, (String){&sep_char, 1});
        }
        buffer_put_string(&writer, entry->name);
    }
    String result = buffer_end_writer(dest, writer, true);
    return result;
}
