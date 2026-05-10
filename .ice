args := "-march=native -masm=intel -std=c99 -fno-builtin -fno-signed-char"

PARALLEL_BENCH_EXE_NAME :: "parallel-bench.exe"
GDI_WINDOW_EXE_NAME :: "gdi-window.exe"
DXGI_WINDOW_EXE_NAME :: "dxgi-window.exe"
run:
  clang $$args src/main.c -o "$$PARALLEL_BENCH_EXE_NAME"
  ./$$PARALLEL_BENCH_EXE_NAME
run-gdi:
  clang $$args src/gdi_window.c -o "$$GDI_WINDOW_EXE_NAME"
  ./$$GDI_WINDOW_EXE_NAME
run-dxgi:
  clang $$args src/dxgi_window.c -o "$$DXGI_WINDOW_EXE_NAME"
  ./$$DXGI_WINDOW_EXE_NAME
