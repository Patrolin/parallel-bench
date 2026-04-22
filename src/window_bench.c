#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/time.h"
#include "lib/window.h"

usize prev_cycles = 0;
isize __stdcall window_proc(WindowHandle window, u32 type, usize wParam, isize lParam) {
  isize result = 0;
  switch (type) {
  case WM_ACTIVATEAPP: {
    // printfln("WM_ACTIVATEAPP: %, %", usize, wParam, isize, lParam);
  } break;
  case WM_KEYDOWN: {
    usize cycles = read_cycle_counter();
    printfln("WM_KEYDOWN: %", usize, (cycles - prev_cycles) / (3800 * Kilo));
    prev_cycles = cycles;
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
  prev_cycles = read_cycle_counter();
  while (window_dispatch_message());
}
