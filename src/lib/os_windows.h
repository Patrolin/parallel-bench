#pragma once
#include "builtin.h"

// types
typedef CINT BOOL;
typedef u64 QWORD;
typedef u32 DWORD;
typedef u16 WORD;

typedef uint16_t wchar;
#define wcstring  wchar *
#define rwcstring readonly wchar *
STRUCT(wstring) {
  rwcstring ptr;
  usize size;
};

#if ARCH_IS_64_BIT
  #define WINAPI
#elif ARCH_IS_32_BIT
  #define WINAPI TODO_32_BIT
#endif
#if NOLIBC
/* NOTE: Windows is dumb... */
CINT _fltused = 0;
#endif

STRUCT(SECURITY_ATTRIBUTES) {
  DWORD nLength;
  rawptr lpSecurityDescriptor;
  BOOL bInheritHandle;
};

#define TIME_INFINITE (DWORD)(-1)
typedef enum : DWORD {
  WAIT_OBJECT_0 = 0,
  WAIT_ABANDONED = 0x80,
  WAIT_TIMEOUT = 0x102,
  WAIT_FAILED = -1,
} WaitResult;

// common
DISTINCT(uptr, Handle);
DISTINCT(Handle, FileHandle);
#define INVALID_HANDLE (Handle)(-1)
foreign bool CloseHandle(Handle handle);
foreign bool WriteFile(FileHandle file, rcstring buffer, DWORD buffer_size, DWORD *bytes_written, rawptr overlapped);

// windows utils
foreign DWORD GetLastError();
foreign WaitResult WaitForSingleObject(Handle handle, DWORD milliseconds);
usize copy_string_to_cwstring(readonly string str, wcstring buffer, usize buffer_size) {
  assert(buffer_size >= 2 * (str.size + 1));
  usize i = 0, j = 0;
  while (i < str.size) {
    // parse utf-8
    u32 codepoint = u32(str.ptr[i]);
    u32 byte_count = u32(count_leading_ones(u8, codepoint));
    codepoint = codepoint & (0xff >> byte_count);
    if (byte_count == 0) byte_count = 1;
    byte_count = min(byte_count, u32(str.size - i));
    if (byte_count >= 2) codepoint = (codepoint << 6) | (str.ptr[i + 1] & 0x3f);
    if (byte_count >= 3) codepoint = (codepoint << 6) | (str.ptr[i + 2] & 0x3f);
    if (byte_count >= 4) codepoint = (codepoint << 6) | (str.ptr[i + 3] & 0x3f);
    i += byte_count;
    // write utf-16
    if (codepoint < 0x10000) {
      buffer[j++] = u16(codepoint);
    } else {
      u32 diff = codepoint - 0x10000;
      u16 high = u16(diff >> 10) | 0xd800;
      u16 low = u16(diff & 0x3ff) | 0xdc00;
      buffer[j++] = high;
      buffer[j++] = low;
    }
  }
  buffer[j++] = 0;
  return j;
}

// linker flags
#if NOLIBC
  #pragma comment(linker, "/ENTRY:_start")
#endif
#if RUN_WITHOUT_CONSOLE
  /* NOTE: /SUBSYSTEM:WINDOWS cannot connect to a console without a race condition, or spawning a new window */
  #pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#else
  #pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif
#pragma comment(lib, "Kernel32.lib")
