args := "-march=native -masm=intel -std=c99 -fno-builtin -fno-signed-char"

BENCH_EXE_NAME :: "parallel-bench.exe"
WINDOW_EXE_NAME :: "window-bench.exe"
DXGI_WINDOW_EXE_NAME :: "dxgi-window.exe"
run:
  clang $$args src/main.c -o "$$BENCH_EXE_NAME"
  ./$$BENCH_EXE_NAME
run-window:
  clang $$args src/window_bench.c -o "$$WINDOW_EXE_NAME"
  ./$$WINDOW_EXE_NAME
run-dxgi:
  clang $$args src/dxgi_window.c -o "$$DXGI_WINDOW_EXE_NAME"
  ./$$DXGI_WINDOW_EXE_NAME
