#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/mem.h"
#include "lib/threads.h"

// timings
#define WARMUP_COUNT                100
#define REPEAT_COUNT                10 * Mega
#define REPEAT_GROUP_SIZE           1000
#define repeat(user_data, callback) repeat_impl(t, user_data, callback, string(#callback));
void repeat_impl(Thread t, rawptr user_data, void (*callback)(Thread t, rawptr user_data), string name) {
  // get the repeat count
  u64 threads_start = global_threads.thread_infos[t].threads_start;
  u64 threads_end = global_threads.thread_infos[t].threads_end;
  u64 thread_count = threads_end - threads_start;
  u64 repetition_count = (REPEAT_COUNT / thread_count) + (t > (REPEAT_COUNT % thread_count));
  // repeat n times
  u64 cycles_count = 0;
  u64 cycles_sum = 0;
  u64 cycles_max = 0;
  for (u64 i = 0; i < repetition_count + WARMUP_COUNT; i += REPEAT_GROUP_SIZE) {
    // time `REPEAT_GROUP_SIZE` runs
    u64 cycles_times[REPEAT_GROUP_SIZE];
    for (u64 j = 0; j < REPEAT_GROUP_SIZE; j++) {
      u64 cycles_before = read_cycle_counter();
      callback(t, user_data);
      u64 cycles_after = read_cycle_counter();
      cycles_times[j] = cycles_after - cycles_before;
    }
    // sort the times
    for (u64 j = 1; j < REPEAT_GROUP_SIZE; j++) {
      u64 k = j;
      u64 current = cycles_times[k];
      for (; k > 0; k--) {
        u64 prev = cycles_times[k - 1];
        if (prev > current) cycles_times[k] = prev;
        else break;
      }
      cycles_times[k] = current;
    }
    // store the lowest 50% (don't time OS interrupts)
    for (u64 j = 0; j < REPEAT_GROUP_SIZE / 2; j++) {
      cycles_count += 1;
      u64 dcycles = cycles_times[j];
      cycles_sum += dcycles;
      if (dcycles > cycles_max) cycles_max = dcycles;
    }
  }
  // print result
  if (single_core(t)) {
    f64 cycles_mean = f64(cycles_sum) / f64(cycles_count);
    printfln("%: % cy (% cy)", string, name, u64, u64(cycles_mean), u64, cycles_max);
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
  // logical_core_count = 64;
  _start_threads(logical_core_count);
}
