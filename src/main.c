#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/mem.h"
#include "lib/threads.h"

// timings
#define WARMUP_COUNT                100
#define REPEAT_COUNT                10 * Mega
#define repeat(user_data, callback) repeat_impl(t, user_data, callback, string(#callback));
void repeat_impl(Thread t, rawptr user_data, void (*callback)(Thread t, rawptr user_data), string name) {
  // warmup the instruction cache
  for (i32 i = 0; i < WARMUP_COUNT; i++) callback(t, user_data);
  // get the repeat count
  u64 threads_start = global_threads.thread_infos[t].threads_start;
  u64 threads_end = global_threads.thread_infos[t].threads_end;
  u64 thread_count = threads_end - threads_start;
  u64 repetition_count = (REPEAT_COUNT / thread_count) + (t > (REPEAT_COUNT % thread_count));
  // repeat n times
  u64 cycles_start = read_cycle_counter();
  for (u64 i = 0; i < repetition_count; i++) {
    callback(t, user_data);
  }
  barrier(t);
  u64 cycles_end = read_cycle_counter();
  // print result
  if (single_core(t)) {
    f64 cycles_per_iteration = f64(cycles_end - cycles_start) / f64(REPEAT_COUNT);
    printfln("%: % cy", string, name, u64, u64(cycles_per_iteration));
  }
  barrier(t);
}

// benchmarks
STRUCT(ArenaAllocator) {
  uptr next;
  uptr end;
};
void do_nothing() {}
void arena_alloc(Thread t, rawptr user_data) {
  ArenaAllocator *arena = (ArenaAllocator *)(user_data);
  uptr size = 1;
  byte *ptr = (byte *)(atomic_fetch_add(&arena->next, size));
  assert(uptr(ptr + size) < arena->end);
  *ptr = 0;
}

void thread_main(Thread t) {
  ArenaAllocator *arena = stack_alloc(ArenaAllocator);
  if (single_core(t)) {
    // make arena
    Bytes buffer = page_reserve(GibiByte);
    for (iptr i = 0; i < iptr(buffer.size); i += 4 * KibiByte) {
      atomic_store(&buffer.ptr[i], 0);
    }
    *arena = (ArenaAllocator){uptr(buffer.ptr), uptr(buffer.ptr + buffer.size)};
  }
  barrier_scatter(t, &arena);

  repeat(0, do_nothing);
  repeat(arena, arena_alloc);
}
int main() {
  // init state
  _init_console();
  _init_page_fault_handler();
  // start threads and do work
  u32 logical_core_count = _get_logical_core_count();
  _start_threads(logical_core_count);
}
