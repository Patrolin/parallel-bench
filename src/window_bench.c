#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/time.h"
#include "lib/window.h"

i64 prev_time = 0;
isize __stdcall window_proc(WindowHandle window, u32 type, usize wParam, isize lParam) {
  isize result = 0;
  switch (type) {
  case WM_ACTIVATEAPP: {
    prev_time = window_message_time;
    // printfln("WM_ACTIVATEAPP: %, %", usize, wParam, isize, lParam);
  } break;
  case WM_KEYDOWN: {
    i64 dnanos = window_message_time - prev_time;
    i64 round_to_ms = 50 * Mega;
    dnanos = ((dnanos + (round_to_ms - 1)) / round_to_ms) * round_to_ms;
    printfln("WM_KEYDOWN: % ms", i64, dnanos / Mega);
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

int main() {
  WindowHandle window = window_open((WindowOptions){
    .className = L"window_class1",
    .title = L"Title",
    .callback = window_proc,
  });
  i64 until_ns = time_ns();
  for (;;) {
    until_ns += Giga / 60;
    while (window_dispatch_message(until_ns));
    // printfln("tick: %", i64, (until_ns / Mega) % 1000);
  }
}
