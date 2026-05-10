#pragma once
#include "builtin.h"
#include "os.h"
#include "time.h"

// types
DISTINCT(CursorHandle, Handle);
DISTINCT(WindowClassHandle, rwcstring);
DISTINCT(WindowHandle, Handle);
DISTINCT(MonitorHandle, Handle);
#if OS_WINDOWS
  #define CS_VREDRAW               0x0001
  #define CS_HREDRAW               0x0002
  #define CS_DBLCLKS               0x0008
  #define CS_OWNDC                 0x0020
  #define WS_OVERLAPPED            0x00000000
  #define WS_VISIBLE               0x10000000
  #define WS_CAPTION               0x00C00000
  #define WS_SYSMENU               0x00080000
  #define WS_THICKFRAME            0x00040000
  #define WS_MINIMIZEBOX           0x00020000
  #define WS_MAXIMIZEBOX           0x00010000
  #define WS_OVERLAPPEDWINDOW      (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
  #define CW_USEDEFAULT            ((i32)0x80000000)
  #define WM_SIZE                  0x0005
  #define WM_ACTIVATE              0x0006
  #define WM_CLOSE                 0x0010
  #define WM_INPUT                 0x00FF
  #define WM_KEYDOWN               0x0100
  #define WM_KEYUP                 0x0101
  #define QS_ALLEVENTS             0x1cbf
  #define PM_REMOVE                0x1
  #define WA_INACTIVE              0
  #define WA_ACTIVE                1
  #define WA_CLICKACTIVE           2
  #define GWL_STYLE                (-16)
  #define HWND_TOP                 0
  #define SWP_NOSIZE               0x0001
  #define SWP_NOMOVE               0x0002
  #define SWP_NOZORDER             0x0004
  #define SWP_FRAMECHANGED         0x0020
  #define SWP_NOOWNERZORDER        0x0200
  #define MONITOR_DEFAULTTONEAREST 0x00000002
  #define VK_F11                   0x7A

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
STRUCT(MSG) {
  WindowHandle window;
  u32 message;
  usize wParam;
  isize lParam;
  DWORD time;
  POINT pt;
};
STRUCT(WINDOWPLACEMENT) {
  u32 length;
  u32 flags;
  u32 showCmd;
  POINT ptMinPosition;
  POINT ptMaxPosition;
  RECT rcNormalPosition;
};
STRUCT(MONITORINFO) {
  DWORD cbSize;
  RECT rcMonitor;
  RECT rcWork;
  DWORD dwFlags;
};
#endif

// syscalls
#if OS_WINDOWS
foreign CursorHandle LoadCursorA(rawptr hInstance, wcstring lpCursorName);
foreign WindowClassHandle RegisterClassW(WNDCLASSW *options);
foreign WindowHandle CreateWindowExW(
  DWORD exStyle,
  rwcstring window_class_name,
  rwcstring window_name,
  DWORD style,
  i32 X,
  i32 Y,
  i32 width,
  i32 height,
  WindowHandle parent,
  rawptr menu,
  rawptr instance,
  rawptr lParam);
foreign isize DefWindowProcW(WindowHandle window, u32 type, usize wParam, isize lParam);
foreign i32 GetMessageW(MSG *message, WindowHandle window, u32 messageFilterMin, u32 messageFilterMax);
foreign BOOL PeekMessageW(MSG *message, WindowHandle window, u32 messageFilterMin, u32 messageFilterMax, u32 removeMsg);
foreign DWORD MsgWaitForMultipleObjects(DWORD handles_count, readonly Handle handles, BOOL wait_for_all, DWORD ms, DWORD wake_mask);
foreign BOOL TranslateMessage(readonly MSG *message);
foreign isize DispatchMessageW(readonly MSG *message);
foreign MonitorHandle MonitorFromWindow(WindowHandle window, u32 flags);
foreign BOOL GetMonitorInfoW(MonitorHandle monitor, MONITORINFO *lpmi);
foreign i32 GetWindowLongW(WindowHandle window, CINT index);
foreign i32 SetWindowLongW(WindowHandle window, CINT index, i32 value);
foreign BOOL GetWindowPlacement(WindowHandle window, WINDOWPLACEMENT *placement);
foreign BOOL SetWindowPlacement(WindowHandle window, readonly WINDOWPLACEMENT *placement);
foreign BOOL SetWindowPos(
  WindowHandle window,
  WindowHandle window_after,
  CINT x,
  CINT y,
  CINT width,
  CINT height,
  u32 flags);
#endif

STRUCT(WindowOptions) {
  rwcstring className;
  rwcstring title;
  WindowEventCallback callback;
  i32 width;
  i32 height;
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
  i32 width = options.width != 0 ? options.width : CW_USEDEFAULT;
  i32 height = options.height != 0 ? options.height : CW_USEDEFAULT;
  WindowHandle window = CreateWindowExW(0, window_class, options.title, window_style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, 0, 0);
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
    while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
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
#if OS_WINDOWS
WINDOWPLACEMENT g_prev_window_placement = {sizeof(g_prev_window_placement)};
#endif
void window_toggle_fullscreen(WindowHandle window) {
#if OS_WINDOWS
  i32 dwStyle = GetWindowLongW(window, GWL_STYLE);
  if (dwStyle & WS_OVERLAPPEDWINDOW) {
    MONITORINFO mi = {sizeof(mi)};
    if (GetWindowPlacement(window, &g_prev_window_placement) && GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &mi)) {
      SetWindowLongW(window, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
  } else {
    SetWindowLongW(window, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(window, &g_prev_window_placement);
    // SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
#else
  assert(false);
#endif
}
