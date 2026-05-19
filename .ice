cppargs := "-march=native -masm=intel -fno-builtin -fno-signed-char -g"
cargs := "-std=c99 -march=native -masm=intel -fno-builtin -fno-signed-char"

run:
  PARALLEL_BENCH_EXE_NAME :: "parallel_bench.exe"
  clang $$args src/main.c -o "$$PARALLEL_BENCH_EXE_NAME"
  ./$$PARALLEL_BENCH_EXE_NAME
run-gdi:
  GDI_WINDOW_EXE_NAME :: "gdi_window.exe"
  clang $$args src/gdi_window.c -o "$$GDI_WINDOW_EXE_NAME"
  ./$$GDI_WINDOW_EXE_NAME
run-dxd12-cpp:
  DXD12_CPP_WINDOW_EXE_NAME :: "dxd12_cpp_window.exe"
  clang $$cppargs src/dxd12_window.cpp -o "$$DXD12_CPP_WINDOW_EXE_NAME"
  ./$$DXD12_CPP_WINDOW_EXE_NAME
run-audio:
  AUDIO_EXE_NAME :: "audio.exe"
  clang $$cppargs src/audio.c -o "$$AUDIO_EXE_NAME"
  ./$$AUDIO_EXE_NAME
