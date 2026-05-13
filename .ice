cppargs := "-march=native -masm=intel -fno-builtin -fno-signed-char"
cargs := "-std=c99 -march=native -masm=intel -fno-builtin -fno-signed-char"

PARALLEL_BENCH_EXE_NAME :: "parallel_bench.exe"
GDI_WINDOW_EXE_NAME :: "gdi_window.exe"
DXD12_CPP_WINDOW_EXE_NAME :: "dxd12_cpp_window.exe"
run:
  clang $$args src/main.c -o "$$PARALLEL_BENCH_EXE_NAME"
  ./$$PARALLEL_BENCH_EXE_NAME
run-gdi:
  clang $$args src/gdi_window.c -o "$$GDI_WINDOW_EXE_NAME"
  ./$$GDI_WINDOW_EXE_NAME
run-dxd12-cpp:
  clang $$cppargs src/dxd12_window.cpp -o "$$DXD12_CPP_WINDOW_EXE_NAME"
  ./$$DXD12_CPP_WINDOW_EXE_NAME
