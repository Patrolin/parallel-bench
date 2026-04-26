#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/time.h"
#include "lib/window.h"

isize __stdcall window_proc(WindowHandle window, u32 type, usize wParam, isize lParam) {
  isize result = 0;
  switch (type) {
  case WM_ACTIVATEAPP: {
    // on focus
    // printfln("WM_ACTIVATEAPP: %, %", usize, wParam, isize, lParam);
  } break;
  case WM_CLOSE: {
    exit_process(0);
  } break;
  case WM_KEYDOWN: {
    i64 dns = window_message_ns;
    i64 round_to_ms = 50 * Mega;
    dns = ((dns + (round_to_ms - 1)) / round_to_ms) * round_to_ms;
    printfln("WM_KEYDOWN: % ms", i64, dns / Mega);
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
  i64 next_frame_ns = time_get_ns();
  window_message_ns = next_frame_ns;
  for (;;) {
    window_dispatch_messages_until_next_frame(&next_frame_ns, 60);
    // printfln("tick: % ms", i64, (next_frame_ns / Mega) % 1000);
  }
}
