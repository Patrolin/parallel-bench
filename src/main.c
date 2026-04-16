#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/mem.h"
#include "lib/threads.h"

// timings
#define WARMUP_COUNT                100
#define REPEAT_COUNT                10 * Mega
#define repeat(user_data, callback) repeat_impl(t, user_data, callback, string(#callback));
u64 repeat_i = 0;
void repeat_impl(Thread t, rawptr user_data, void (*callback)(Thread t, rawptr user_data), string name) {
  // get shared ptr
  for (i32 i = 0; i < WARMUP_COUNT; i++) callback(t, user_data);
  u64 *i_ptr = stack_alloc(u64);
  *i_ptr = 0;
  barrier_scatter(t, i_ptr);
  // repeat n times
  u64 cycles_start = read_cycle_counter();
  while (true) {
    u64 i = atomic_fetch_add(i_ptr, 1);
    if (i >= REPEAT_COUNT) break;
    callback(t, user_data);
  }
  u64 cycles_end = read_cycle_counter();
  barrier(t);
  // print result
  barrier(t);

  if (single_core(t)) {
    f64 cycles_per_iteration = f64(cycles_end - cycles_start) / f64(REPEAT_COUNT);
    printfln("%: % cy", string, name, u64, u64(cycles_per_iteration));
  }
  barrier(t);
}

// benchmarks
void do_nothing() {}
STRUCT(ArenaAllocator) {
  uptr next;
  uptr end;
};
void arena_alloc(Thread t, rawptr user_data) {
  ArenaAllocator *arena = (ArenaAllocator *)(user_data);
  uptr size = 1;
  byte *ptr = (byte *)(atomic_fetch_add(&arena->next, size));
  // printfln("arena_alloc: %, %", hex, arena, hex, ptr, hex, arena->end);
  assert(uptr(ptr + size) < arena->end);
  *ptr = 0;
}

void thread_main(Thread t) {
  // alloc buffer
  Bytes *buffer = stack_alloc(Bytes);
  ArenaAllocator *arena = stack_alloc(ArenaAllocator);
  if (single_core(t)) {
    *buffer = page_reserve(GibiByte);
    *arena = (ArenaAllocator){uptr(buffer->ptr), uptr(buffer->ptr + buffer->size)};
    // make sure the pages are commited
    for (iptr i = 0; i < iptr(buffer->size); i += 4 * KibiByte) {
      atomic_store(&buffer->ptr[i], 0);
    }
  }
  barrier_scatter(t, &buffer);
  barrier_scatter(t, &arena);

  repeat(0, do_nothing);
  repeat(arena, arena_alloc);
}
int main() {
  // alloc buffer
  _init_console();
  _init_page_fault_handler();
  // start threads and do work
  u32 logical_core_count = _get_logical_core_count();
  logical_core_count = 2;
  _start_threads(logical_core_count);
}
