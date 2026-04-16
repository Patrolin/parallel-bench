#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/mem.h"
#include "lib/threads.h"

void thread_main(Thread t) {
  printfln("t: %", u32, t);
}
int main() {
  // alloc buffer
  _init_console();
  _init_page_fault_handler();
  Bytes buffer = page_reserve(GibiByte);
  // make sure it is commited
  for (iptr i = 0; i < iptr(buffer.size); i += 4 * KibiByte) {
    atomic_store(&buffer.ptr[i], 0);
  }
  // start threads and do work
  _start_threads(_get_logical_core_count());
}
