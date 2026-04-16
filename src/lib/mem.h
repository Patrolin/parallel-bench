#pragma once
#include "builtin.h"
#include "os.h"

// params
#define RING_BUFFER_SIZE 4096
ASSERT_POWER_OF_TWO(RING_BUFFER_SIZE);

// page fault handler
#if OS_WINDOWS
typedef enum : DWORD {
  EXCEPTION_ACCESS_VIOLATION = 0xC0000005,
} ExceptionCode;
#define EXCEPTION_MAXIMUM_PARAMETERS 15
STRUCT(EXCEPTION_RECORD) {
  ExceptionCode ExceptionCode;
  DWORD ExceptionFlags;
  struct EXCEPTION_RECORD *ExceptionRecord;
  rawptr ExceptionAddress;
  DWORD NumberParameters;
  uptr ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
};

OPAQUE(CONTEXT);
STRUCT(_EXCEPTION_POINTERS) {
  EXCEPTION_RECORD *ExceptionRecord;
  CONTEXT *ContextRecord;
};
typedef enum : DWORD {
  EXCEPTION_EXECUTE_HANDLER = 1,
  EXCEPTION_CONTINUE_SEARCH = 0,
  EXCEPTION_CONTINUE_EXECUTION = -1,
} ExceptionResult;
typedef ExceptionResult ExceptionFilter(_EXCEPTION_POINTERS *exception);

typedef enum : DWORD {
  MEM_COMMIT = 1 << 12,
  MEM_RESERVE = 1 << 13,
  MEM_DECOMMIT = 1 << 14,
  MEM_RELEASE = 1 << 15,
} AllocTypeFlags;
typedef enum : DWORD {
  PAGE_READWRITE = 1 << 2,
} AllocProtectFlags;

// foreign ExceptionFilter* SetUnhandledExceptionFilter(ExceptionFilter
// filter_callback);
foreign Handle AddVectoredExceptionHandler(uptr run_first,
                                           ExceptionFilter handler);
foreign uptr VirtualAlloc(uptr address, usize size, AllocTypeFlags type_flags,
                          AllocProtectFlags protect_flags);
foreign BOOL VirtualFree(uptr address, usize size, AllocTypeFlags type_flags);
#elif OS_LINUX
typedef enum : u32 {
  PROT_EXEC = 1 << 0,
  PROT_READ = 1 << 1,
  PROT_WRITE = 1 << 2,
} ProtectionFlags;
typedef enum : u32 {
  MAP_PRIVATE = 1 << 1,
  MAP_ANONYMOUS = 1 << 5,
  MAP_GROWSDOWN = 1 << 8,
  MAP_STACK = 1 << 17,
} AllocTypeFlags;

uptr mmap(rawptr address, usize size, ProtectionFlags protection_flags,
          AllocTypeFlags type_flags, FileHandle fd, usize offset) {
  return syscall6(SYS_mmap, (uptr)address, size, protection_flags, type_flags,
                  (uptr)fd, offset);
}
uptr munmap(uptr address, usize size) {
  return syscall2(SYS_munmap, (uptr)address, size);
}
#endif

// page alloc
#if OS_WINDOWS
ExceptionResult _page_fault_handler(_EXCEPTION_POINTERS *exception_info) {
  EXCEPTION_RECORD *exception = exception_info->ExceptionRecord;
  DWORD exception_code = exception->ExceptionCode;
  if (expect_near(exception_code == EXCEPTION_ACCESS_VIOLATION)) {
    uptr ptr = exception->ExceptionInformation[1];
    uptr page_ptr = ptr & ~uptr(OS_PAGE_SIZE - 1);
    uptr commited_ptr =
        VirtualAlloc(page_ptr, OS_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    return page_ptr != 0 && commited_ptr != 0 ? EXCEPTION_CONTINUE_EXECUTION
                                              : EXCEPTION_EXECUTE_HANDLER;
  }
  return EXCEPTION_EXECUTE_HANDLER;
}
void _init_page_fault_handler() {
  AddVectoredExceptionHandler(1, _page_fault_handler);
}
#else
#define _init_page_fault_handler()
#endif

Bytes page_reserve(usize size) {
  Bytes buffer;
  buffer.size = size;
#if OS_WINDOWS
  buffer.ptr = (byte *)VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
  assert(buffer.ptr != 0);
#elif OS_LINUX
  buffer.ptr = (byte *)mmap(0, size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(iptr(buffer.ptr) != -1);
#else
  assert(false);
#endif
  return buffer;
}
void page_free(uptr ptr) {
#if OS_WINDOWS
  assert(VirtualFree(ptr, 0, MEM_RELEASE));
#elif OS_LINUX
  assert(munmap(ptr, 0) == 0);
#else
  assert(false);
#endif
}
