args := "-march=native -masm=intel -std=c99 -fno-builtin -fno-signed-char"

BENCH_EXE_NAME :: "parallel-bench.exe"
WINDOW_EXE_NAME :: "window-bench.exe"
run:
  clang $$args src/main.c -o "$$BENCH_EXE_NAME"
  ./$$BENCH_EXE_NAME
window:
  clang $$args src/window_bench.c -o "$$WINDOW_EXE_NAME"
  ./$$WINDOW_EXE_NAME
