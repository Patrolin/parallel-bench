#pragma once
#include "builtin.h"
#include "os.h"

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
  #define WM_ACTIVATEAPP      0x001C
  #define WM_INPUT            0x00FF
  #define WM_KEYDOWN          0x0100
  #define WM_KEYUP            0x0101

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
bool window_dispatch_message() {
#if OS_WINDOWS
  MSG message;
  int result = GetMessageW(&message, 0, 0, 0);
  assert(result >= 0);
  TranslateMessage(&message);
  DispatchMessageW(&message);
#else
  assert(false);
#endif
  return true;
}
