args := "-march=native -masm=intel -std=c99 -fno-builtin -fno-signed-char"

EXE_NAME :: "parallel-bench.exe"
run:
  clang $$args src/main.c -o "$$EXE_NAME"
  ./$$EXE_NAME
