#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/mem.h"
#include "lib/threads.h"

#define REPEAT_COUNT     10 * Mega
#define repeat(callback) repeat_impl(t, callback, string(#callback))
void repeat_impl(Thread t, void (*callback)(u64 i), string name) {
  // get shared ptr
  u64 *i_ptr = stack_alloc(u64);
  barrier_scatter(t, &i_ptr);
  // repeat n times
  u64 cycles_start = read_cycle_counter();
  while (true) {
    u64 i = atomic_fetch_add(i_ptr, 1);
    if (i >= REPEAT_COUNT) break;
    callback(i);
  }
  u64 cycles_end = read_cycle_counter();
  barrier(t);
  // print result
  f64 cycles_per_iteration = f64(cycles_end - cycles_start) / f64(REPEAT_COUNT);
  if (single_core(t)) {
    printfln("%: % cy", string, name, u64, u64(cycles_per_iteration));
  }
  barrier(t);
}

void do_nothing() {}
void thread_main(Thread t) {
  repeat(do_nothing);
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
  u32 logical_core_count = _get_logical_core_count();
  _start_threads(logical_core_count);
}
