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
  case WM_CLOSE: {
    exit_process(0);
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
  window_message_time = time_ns();
  i64 next_frame_ns = window_message_time;
  for (;;) {
    // handle window events until the next frame
    window_dispatch_messages(&next_frame_ns, 60);
    // do something this frame
    printfln("tick: %", i64, next_frame_ns);
  }
}
