#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/time.h"
#include "lib/window.h"

i32 g_window_width;
i32 g_window_height;
i64 g_prev_ns;

isize __stdcall window_proc(WindowHandle window, u32 type, usize wParam, isize lParam) {
  isize result = 0;
  switch (type) {
  case WM_ACTIVATE: {
    if (wParam != WA_INACTIVE) {
      g_prev_ns = atomic_load(&window_message_ns);
    }
  } break;
  case WM_SIZE: {
    g_window_width = u32(lParam) & MAX(u16);
    g_window_height = u32(lParam) >> 16;
  } break;
  case WM_CLOSE: {
    exit_process(0);
  } break;
  case WM_KEYDOWN: {
    if (wParam == VK_F11) {
      window_toggle_fullscreen(window);
    } else {
      i64 dns = window_message_ns - g_prev_ns;
      i64 round_to_ms = 50 * Mega;
      dns = ((dns + (round_to_ms - 1)) / round_to_ms) * round_to_ms;
      printfln("WM_KEYDOWN: % ms", i64, dns / Mega);
    }
  } break;
  case WM_KEYUP: {
    // printfln("WM_KEYUP: %, %", usize, wParam, isize, lParam);
  } break;
  default: {
    result = DefWindowProcW(window, type, wParam, lParam);
  } break;
  }
  return result;
}

DISTINCT(DCHandle, Handle);
STRUCT(PAINTSTRUCT) {
  DCHandle hdc;
  BOOL fErase;
  RECT rcPaint;
  BOOL fRestore;
  BOOL fIncUpdate;
  byte rgbReserved[32];
};
foreign DCHandle GetDC(WindowHandle window);
STRUCT(BITMAPINFOHEADER) {
  DWORD biSize;
  long biWidth;
  long biHeight;
  WORD biPlanes;
  WORD biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  long biXPelsPerMeter;
  long biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
};
STRUCT(RGBQUAD) {
  byte rgbBlue;
  byte rgbGreen;
  byte rgbRed;
  byte rgbReserved;
};
STRUCT(BITMAPINFO) {
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD bmiColors[1];
};
foreign int SetDIBitsToDevice(
  DCHandle hdc,
  int xDest,
  int yDest,
  DWORD w,
  DWORD h,
  int xSrc,
  int ySrc,
  u32 StartScan,
  u32 cLines,
  const void *lpvBits,
  const BITMAPINFO *lpbmi,
  u32 ColorUse);
#define BI_RGB         0
#define DIB_RGB_COLORS 0
#pragma comment(lib, "Gdi32.lib")

u32 buffer[400 * 400];
int main() {
  i64 next_frame_ns = time_get_ns();
  window_message_ns = next_frame_ns;
  WindowHandle window = window_open((WindowOptions){
    .className = L"window_class1",
    .title = L"Window bench",
    .callback = window_proc,
    .width = 256 + 16,
    .height = 256 + 39,
  });
  printfln("width: %, height: %", i32, g_window_width, i32, g_window_height);
  DCHandle dc = GetDC(window);
  for (;;) {
    // handle inputs
    window_dispatch_messages_until_next_frame(&next_frame_ns, 60);
    // printfln("tick: % ms", i64, (next_frame_ns / Mega) % 1000);

    // render
    printfln("g_window_width: %, g_window_height: %", i32, g_window_width, i32, g_window_height);
    for (usize i = 0; i < sizeof(buffer) / 4; i++) {
      /* NOTE: BGRA */
      buffer[i] = 0x7f007fff;
    }
    // present
    BITMAPINFO bmi = {
      .bmiHeader = {
        .biSize = sizeof(bmi.bmiHeader),
        .biWidth = g_window_width,
        .biHeight = -g_window_height,
        .biPlanes = 1,
        .biBitCount = 32,
        .biCompression = BI_RGB,
      },
    };
    i32 result = SetDIBitsToDevice(dc, 0, 0, u32(g_window_width), u32(g_window_height), 0, 0, 0, u32(g_window_height), buffer, &bmi, DIB_RGB_COLORS);
    assert(result > 0);
  }
}
