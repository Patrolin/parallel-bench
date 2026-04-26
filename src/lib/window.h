#pragma once
#include "builtin.h"
#include "os.h"
#include "time.h"

// types
DISTINCT(Handle, CursorHandle);
DISTINCT(rwcstring, WindowClassHandle);
DISTINCT(Handle, WindowHandle);
#if OS_WINDOWS
  #define CS_VREDRAW          0x0001
  #define CS_HREDRAW          0x0002
  #define CS_DBLCLKS          0x0008
  #define CS_OWNDC            0x0020
  #define WS_OVERLAPPED       0x00000000L
  #define WS_VISIBLE          0x10000000L
  #define WS_CAPTION          0x00C00000L
  #define WS_SYSMENU          0x00080000L
  #define WS_THICKFRAME       0x00040000L
  #define WS_MINIMIZEBOX      0x00020000L
  #define WS_MAXIMIZEBOX      0x00010000L
  #define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
  #define CW_USEDEFAULT       ((i32)0x80000000)
  #define WM_CLOSE            0x0010
  #define WM_ACTIVATEAPP      0x001C
  #define WM_INPUT            0x00FF
  #define WM_KEYDOWN          0x0100
  #define WM_KEYUP            0x0101
  #define QS_ALLEVENTS        0x1cbf

typedef isize __stdcall (*WindowEventCallback)(WindowHandle window, u32 type, usize wParam, isize lParam);
STRUCT(WNDCLASSW) {
  u32 style;
  WindowEventCallback lpfnWndProc;
  i32 cbClsExtra;
  i32 cbWndExtra;
  rawptr hInstance;
  rawptr hIcon;
  CursorHandle hCursor;
  rawptr hbrBackground;
  rwcstring lpszMenuName;
  rwcstring lpszClassName;
};
STRUCT(POINT) {
  i32 x;
  i32 y;
};
STRUCT(MSG) {
  WindowHandle window;
  u32 message;
  usize wParam;
  isize lParam;
  DWORD time;
  POINT pt;
};
#endif

// syscalls
#if OS_WINDOWS
foreign CursorHandle LoadCursorA(rawptr hInstance, wcstring lpCursorName);
foreign WindowClassHandle RegisterClassW(WNDCLASSW *options);
foreign WindowHandle CreateWindowExW(
  DWORD dwExStyle,
  rwcstring lpClassName,
  rwcstring lpWindowName,
  DWORD dwStyle,
  i32 X,
  i32 Y,
  i32 nWidth,
  i32 nHeight,
  WindowHandle hWndParent,
  rawptr hMenu,
  rawptr hInstance,
  rawptr lpParam);
foreign isize DefWindowProcW(WindowHandle window, u32 type, usize wParam, isize lParam);
foreign i32 GetMessageW(MSG *message, WindowHandle window, u32 messageFilterMin, u32 messageFilterMax);
foreign BOOL PeekMessageW(MSG *message, WindowHandle window, u32 messageFilterMin, u32 messageFilterMax, u32 removeMsg);
foreign DWORD MsgWaitForMultipleObjects(DWORD handles_count, readonly Handle handles, BOOL wait_for_all, DWORD ms, DWORD wake_mask);
foreign BOOL TranslateMessage(readonly MSG *message);
foreign isize DispatchMessageW(readonly MSG *message);
#endif

STRUCT(WindowOptions) {
  rwcstring className;
  rwcstring title;
  WindowEventCallback callback;
};
WindowHandle window_open(WindowOptions options) {
#if OS_WINDOWS
  CursorHandle cursor = LoadCursorA(0, (wcstring)(32512));
  WNDCLASSW window_class_options = {
    .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = options.callback,
    .lpszClassName = options.className,
    .hCursor = cursor,
  };
  WindowClassHandle window_class = RegisterClassW(&window_class_options);
  assert(window_class != 0);
  DWORD window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  WindowHandle window = CreateWindowExW(0, window_class, options.title, window_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
  assert(window != 0);
  return window;
#else
  assert(false);
#endif
}
i64 window_message_ns;
bool window_dispatch_message(i64 until_ns) {
  i64 time_ns = time_get_ns();
  window_message_ns = time_ns;
  if (time_ns - until_ns > 0) return false;
#if OS_WINDOWS
  MSG message;
  DWORD wait_ms = (DWORD)((until_ns - time_ns) / Mega);
  if (MsgWaitForMultipleObjects(0, 0, false, wait_ms, QS_ALLEVENTS) == WAIT_OBJECT_0) {
    while (PeekMessageW(&message, 0, 0, 0, 0x1)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  return true;
#else
  assert(false);
#endif
}
void window_dispatch_messages_until_next_frame(i64 *next_frame_ns_ptr, i64 fps) {
  // get the next frame time (can be multiple steps due to WM_SIZING on windows...)
  i64 next_frame_ns = *next_frame_ns_ptr;
  while (window_message_ns - next_frame_ns >= 0) {
    next_frame_ns += Giga / fps;
  }
  *next_frame_ns_ptr = next_frame_ns;
  // dispatch messages until the next frame time
  while (window_dispatch_message(next_frame_ns));
}
