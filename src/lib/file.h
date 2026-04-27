#pragma once
#include "builtin.h"
#include "os.h"
#include "os_windows.h"

// types
#if OS_WINDOWS
  #define MOVEFILE_REPLACE_EXISTING 0x00000001

  #define GENERIC_READ    0x80000000L
  #define GENERIC_WRITE   0x40000000L
  #define GENERIC_EXECUTE 0x20000000L
  #define GENERIC_ALL     0x10000000L

  #define FILE_SHARE_READ   0x00000001
  #define FILE_SHARE_WRITE  0x00000002
  #define FILE_SHARE_DELETE 0x00000004

  #define CREATE_NEW        1
  #define CREATE_ALWAYS     2
  #define OPEN_ALWAYS       4
  #define OPEN_EXISTING     3
  #define TRUNCATE_EXISTING 5

  #define FILE_ATTRIBUTE_NORMAL 0x00000080
#elif OS_LINUX
typedef enum : CUINT {
  O_WRONLY = 1 << 0,
  O_RDWR = 1 << 1,
  /* create if not exists */
  O_CREAT = 1 << 6,
  /* don't open if exists */
  O_EXCL = 1 << 7,
  /* truncate */
  O_TRUNC = 1 << 9,
  O_DIRECTORY = 1 << 16,
} FileFlags;
#endif

// syscalls
#if OS_WINDOWS
foreign i32 CreateDirectoryW(rwcstring dir_path, readonly SECURITY_ATTRIBUTES *security);
foreign bool MoveFileExW(rwcstring src_path, rwcstring dest_path, DWORD flags);
foreign FileHandle CreateFileW(rwcstring file_path, DWORD access, DWORD share_mode, SECURITY_ATTRIBUTES *lpSecurityAttributes, DWORD creation_disposition, DWORD flags, Handle template_file);
foreign bool WriteFile(FileHandle file, rcstring buffer, DWORD buffer_size, DWORD *bytes_written, rawptr overlapped);
#elif OS_LINUX
isize open(rcstring path, FileFlags flags, CUINT mode) {
  return syscall3(SYS_open, (uptr)path, flags, mode);
}
#endif

// dir
void create_dir_if_not_exists(string dir_path) {
#if OS_WINDOWS
  wchar wdir_path[dir_path.size + 1];
  copy_string_to_cwstring(dir_path, wdir_path, sizeof(wdir_path));
  assert(CreateDirectoryW(wdir_path, 0) != ERROR_PATH_NOT_FOUND);
#elif OS_LINUX
  char cdir_path[dir_path.size + 1];
  copy_to_cstring(dir_path, cdir_path, sizeof(cdir_path));
  assert(false);
#else
  assert(false);
#endif
}

// file
void move_path_atomically(string src_path, string dest_path) {
#if OS_WINDOWS
  wchar wsrc_path[src_path.size + 1];
  wchar wdest_path[src_path.size + 1];
  copy_string_to_cwstring(src_path, wsrc_path, sizeof(wsrc_path));
  copy_string_to_cwstring(dest_path, wdest_path, sizeof(wdest_path));
  assert(MoveFileExW(wsrc_path, wdest_path, MOVEFILE_REPLACE_EXISTING));
#else
  assert(false);
#endif
}
void write_entire_file_atomically(string file_path, string content) {
  // get temp file path
  string TMP_SUFFIX = string(".tmp");
  char tmp_path_buffer[file_path.size + TMP_SUFFIX.size];
  str_concat(file_path, TMP_SUFFIX, tmp_path_buffer, sizeof(tmp_path_buffer));
  string tmp_file_path = (string){tmp_path_buffer, sizeof(tmp_path_buffer)};
  // open temp file
#if OS_WINDOWS
  wchar wtmp_file_path[tmp_file_path.size + 1];
  copy_string_to_cwstring(tmp_file_path, wtmp_file_path, sizeof(wtmp_file_path));
  FileHandle tmp_file = CreateFileW(wtmp_file_path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
#else
  assert(false);
#endif
  // write to temp file
  u32 total_bytes_written = 0;
  u32 bytes_written;
  for (;;) {
    i64 bytes_to_write = i64(total_bytes_written) - i64(content.size);
    if (bytes_to_write <= 0) break;
#if OS_WINDOWS
    assert(WriteFile(tmp_file, &content.ptr[total_bytes_written], u32(bytes_to_write), &bytes_written, 0));
    total_bytes_written += bytes_written;
#else
    assert(false);
#endif
  }
  // atomically move temp file to file_path
  move_path_atomically(tmp_file_path, file_path);
}

/*
move_path_atomically :: proc(src_path, dest_path: string) {
        when ODIN_OS == .Windows {
                result := MoveFileExW(&copy_string_to_cwstr(src_path)[0],
&copy_string_to_cwstr(dest_path)[0], {.MOVEFILE_REPLACE_EXISTING})
                fmt.assertf(bool(result), "Failed to move path: '%v' to '%v'",
src_path, dest_path) } else when ODIN_OS == .Linux { cbuffer: [2 *
WINDOWS_MAX_PATH]byte = --- csrc_path, cbuffer2 := copy_to_cstring(src_path,
cbuffer[:]) cdest_path, _ := copy_to_cstring(dest_path, cbuffer2) err :=
renameat2(AT_FDCWD, csrc_path, AT_FDCWD, cdest_path) fmt.assertf(err == 0,
"Failed to move path: '%v' to '%v'", src_path, dest_path) } else { assert(false)
        }
}
*/
/* NOTE: We only support up to `wlen(dir) + 1 + wlen(relative_file_path) <
   MAX_PATH (259 utf16 chars + null terminator)`. \
      While we *can* give windows long paths as input, it has no way to return
   long paths back to us. \
      Windows gives us (somewhat) relative paths, so we could theoretically
   extend support to `wlen(relative_file_path) < MAX_PATH`. \ But that doesn't
   really change much.
*/
/*
walk_files :: proc(dir_path: string, callback: proc(path: string, data: rawptr),
data: rawptr = nil) { when ODIN_OS == .Windows { path_to_search :=
fmt.tprint(dir_path, "*", sep = "\\") wpath_to_search :=
copy_string_to_cwstr(path_to_search) find_result: WIN32_FIND_DATAW find :=
FindFirstFileW(&wpath_to_search[0], &find_result) if find !=
FindFile(INVALID_HANDLE) { for { relative_path :=
copy_cwstr_to_string(&find_result.cFileName[0]) assert(relative_path != "")

                                if relative_path != "." && relative_path != ".."
{ is_dir := find_result.dwFileAttributes >= {.FILE_ATTRIBUTE_DIRECTORY}
                                        next_path := fmt.tprint(dir_path,
relative_path, sep = "/") if is_dir { walk_files(next_path, callback, data) }
else { callback(next_path, data)
                                        }
                                }
                                if FindNextFileW(find, &find_result) == false
{break}
                        }
                        FindClose(find)
                }
        } else when ODIN_OS == .Linux {
                cbuffer: [WINDOWS_MAX_PATH]byte = ---
                cdir_path, _ := copy_to_cstring(dir_path, cbuffer[:])
                dir := DirHandle(open(cdir_path, {.O_DIRECTORY}))
                if Errno(dir) == .ENOTDIR {
                        callback(dir_path, data)
                } else {
                        assert(dir >= 0)
                        dir_entries_buffer: [4096]byte
                        bytes_written := get_directory_entries_64b(dir,
&dir_entries_buffer[0], len(dir_entries_buffer)) assert(bytes_written >= 0) if
bytes_written == 0 {return}

                        offset := 0
                        for offset < bytes_written {
                                dirent :=
(^Dirent64)(&dir_entries_buffer[offset]) crelative_path :=
transmute([^]byte)(&dirent.cfile_name) len_lower_bound := max(0,
int(dirent.size) - 28) relative_path := string_from_cstring(crelative_path,
len_lower_bound)

                                if relative_path != "." && relative_path != ".."
{ is_dir := dirent.type == .Dir next_path := fmt.tprint(dir_path, relative_path,
sep = "/") if is_dir { walk_files(next_path, callback, data) } else {
                                                callback(next_path, data)
                                        }
                                }
                                offset += int(dirent.size)
                        }
                }
        } else {
                assert(false)
        }
}

// read
@(require_results)
open_file_for_reading :: proc(file_path: string) -> (file: FileHandle) {
        when ODIN_OS == .Windows {
                wfile_path := copy_string_to_cwstr(file_path)
                file = CreateFileW(
                        &wfile_path[0],
                        {.GENERIC_READ},
                        {.FILE_SHARE_READ, .FILE_SHARE_WRITE},
                        nil,
                        .Open,
                        {.FILE_ATTRIBUTE_NORMAL, .FILE_FLAG_SEQUENTIAL_SCAN},
                )
        } else when ODIN_OS == .Linux {
                cbuffer: [WINDOWS_MAX_PATH]byte = ---
                cfile_path, _ := copy_to_cstring(file_path, cbuffer[:])
                file = open(cfile_path)
                if file < 0 {file = FileHandle(INVALID_HANDLE)}
        } else {
                assert(false)
        }
        return
}
*/
/* NOTE: this can fail if the file gets deleted or whatever */
/*
@(require_results)
get_file_size :: proc(file: FileHandle) -> (file_size: int, ok: bool) {
        when ODIN_OS == .Windows {
                win_file_size: LARGE_INTEGER = ---
                ok = GetFileSizeEx(file, &win_file_size) == true
                file_size = int(win_file_size)
        } else when ODIN_OS == .Linux {
                file_info: Statx = ---
                err := get_file_info(file, {.STATX_SIZE}, &file_info)
                ok = err == 0
                file_size = int(file_info.size)
        } else {
                assert(false)
        }
        return
}
@(require_results)
read_file :: proc(file_path: string, allocator := context.temp_allocator) ->
(text: string, ok: bool) #no_bounds_check {
        // open file
        file := open_file_for_reading(file_path)
        ok = file != FileHandle(INVALID_HANDLE)
        if ok {
                // read file
                sb := string_builder(allocator = allocator)
                buffer: [4096]u8 = ---
                for {
                        when ODIN_OS == .Windows {
                                bytes_read: u32 = ---
                                ReadFile(file, &buffer[0], len(buffer),
&bytes_read, nil) } else when ODIN_OS == .Linux { bytes_read: int = ---
                                bytes_read = read(file, &buffer[0], len(buffer))
                        } else {
                                assert(false)
                        }
                        if bytes_read == 0 {break}
                        fmt.sbprint(&sb, string(buffer[:bytes_read]))
                }
                text = to_string(sb)
                // close file
                close_file(file)
        }
        return
}

// write
open_file_for_writing_and_truncate :: proc(file_path: string) -> (file:
FileHandle, ok: bool) { when ODIN_OS == .Windows { file =
CreateFileW(&copy_string_to_cwstr(file_path)[0], {.GENERIC_WRITE},
{.FILE_SHARE_READ}, nil, .CreateOrOpenAndTruncate, {.FILE_ATTRIBUTE_NORMAL}) ok
= file != FileHandle(INVALID_HANDLE) } else when ODIN_OS == .Linux { cbuffer:
[WINDOWS_MAX_PATH]byte = --- cfile_path, _ := copy_to_cstring(file_path,
cbuffer[:]) file = open(cfile_path, {.O_CREAT, .O_WRONLY, .O_TRUNC}) ok = file
!= FileHandle(INVALID_HANDLE) } else { assert(false)
        }
        return
}
write_to_file :: proc(file: FileHandle, text: string) {
        when ODIN_OS == .Windows {
                assert(len(text) < int(max(u32)))
                bytes_written: DWORD
                WriteFile(file, raw_data(text), u32(len(text)), &bytes_written,
nil) assert(int(bytes_written) == len(text)) } else when ODIN_OS == .Linux {
                bytes_written := write(file, raw_data(text), len(text))
                assert(bytes_written == len(text))
        } else {
                assert(false)
        }
}
flush_file_data_and_metadata :: proc(file: FileHandle) {
        when ODIN_OS == .Windows {
                assert(bool(FlushFileBuffers(file)))
        } else when ODIN_OS == .Linux {
                assert(fsync(file) == 0)
        } else {
                assert(false)
        }
}
flush_file_data :: proc(file: FileHandle) {
        when ODIN_OS == .Windows {
                assert(bool(FlushFileBuffers(file)))
        } else when ODIN_OS == .Linux {
                assert(fdatasync(file) == 0)
        } else {
                assert(false)
        }
}
close_file :: proc(file: FileHandle) {
        when ODIN_OS == .Windows {
                CloseHandle(Handle(file))
        } else when ODIN_OS == .Linux {
                close(file)
        } else {
                assert(false)
        }
}

// os agnostic
write_file_atomically :: proc(file_path, text: string) {
        // write to temp file
        temp_file_path := fmt.tprintf("%v.tmp", file_path)
        temp_file, ok := open_file_for_writing_and_truncate(temp_file_path)
        fmt.assertf(ok, "Failed to open file: '%v'", file_path)
        write_to_file(temp_file, text)
        close_file(temp_file)
        // move temp file to file_path
        move_path_atomically(temp_file_path, file_path)
}
*/
