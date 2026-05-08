#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/time.h"
#include "lib/window.h"

i64 prev_ns;
isize __stdcall window_proc(WindowHandle window, u32 type, usize wParam, isize lParam) {
  isize result = 0;
  switch (type) {
  case WM_ACTIVATE: {
    if (wParam != WA_INACTIVE) {
      prev_ns = atomic_load(&window_message_ns);
    }
  } break;
  case WM_CLOSE: {
    exit_process(0);
  } break;
  case WM_KEYDOWN: {
    if (wParam == VK_F11) {
      window_toggle_fullscreen(window);
    } else {
      i64 dns = window_message_ns - prev_ns;
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

int main() {
  i64 next_frame_ns = time_get_ns();
  window_message_ns = next_frame_ns;
  WindowHandle window = window_open((WindowOptions){
    .className = L"window_class1",
    .title = L"Window bench",
    .callback = window_proc,
  });
  for (;;) {
    window_dispatch_messages_until_next_frame(&next_frame_ns, 60);
    // printfln("tick: % ms", i64, (next_frame_ns / Mega) % 1000);
  }
}
