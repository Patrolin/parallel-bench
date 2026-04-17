#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/mem.h"
#include "lib/time.h"
#include "lib/threads.h"

// params
#define REPEAT_MEAN_COUNT       10000
#define REPEAT_PERCENTILE_COUNT 10000

// timings
#define repeat(user_data, callback) repeat_impl(t, user_data, callback, string(#callback));
void repeat_impl(Thread t, rawptr user_data, void (*callback)(Thread t, rawptr user_data), string name) {
  // measure mean
  u64 cycles_start = read_cycle_counter();
  for (u64 i = 0; i < REPEAT_MEAN_COUNT; i++) {
    callback(t, user_data);
  }
  u64 cycles_sum = read_cycle_counter() - cycles_start;
  /* TODO: the max is completely unreliable because of OS interrupts
    and percentiles lie about the data -> we need to plot the entire cdf
  */
  // measure individual results
  u64 results[REPEAT_PERCENTILE_COUNT];
  for (u64 i = 0; i < REPEAT_PERCENTILE_COUNT; i += 1) {
    u64 cycles_before = read_cycle_counter();
    callback(t, user_data);
    u64 cycles_after = read_cycle_counter();
    results[i] = cycles_after - cycles_before;
  }
  // sort the results // TODO: faster sorting algorithm
  for (u64 j = 1; j < REPEAT_PERCENTILE_COUNT; j++) {
    u64 k = j;
    u64 current = results[k];
    for (; k > 0; k--) {
      u64 prev = results[k - 1];
      if (prev > current) results[k] = prev;
      else break;
    }
    results[k] = current;
  }
  // compute 99th percentile
  u64 cycles_percentile = 0;
  for (u64 i = 0; i < REPEAT_PERCENTILE_COUNT * 99 / 100; i++) {
    u64 dgroup_cycles = results[i];
    if (dgroup_cycles > cycles_percentile) cycles_percentile = dgroup_cycles;
  }
  barrier_gather(t, results[REPEAT_PERCENTILE_COUNT - 1]);
  // print result
  if (single_core(t)) {
    u64 thread_count = global_threads.thread_infos[t].threads_end;
    for (u64 i = 0; i < thread_count; i++) {
      u64 max_value = global_threads.values[i];
      // printfln("MAX[%]: % cy", u64, i, u64, max_value);
    }
    f64 cycles_mean = f64(cycles_sum) / f64(REPEAT_MEAN_COUNT);
    printfln("  %: % cy (% cy)", string, name, u64, u64(cycles_mean), u64, cycles_percentile);
  }
  barrier(t);
}

// benchmarks
STRUCT(TicketMutex) {
  u32 next;
  u32 serving;
};
STRUCT(ArenaAllocator) {
  uptr start;
  uptr next;
  uptr end;
  u32 mutex;
  TicketMutex lock;
};
u32 wait_for_ticket_mutex(TicketMutex *lock) {
  u32 ticket = atomic_fetch_add(&lock->next, 1);
  while (atomic_load(&lock->serving) != ticket) cpu_relax();
  return ticket;
}
void release_ticket_mutex(TicketMutex *lock, u32 ticket) {
  volatile_store(&lock->serving, ticket + 1);
}
void wait_for_mutex(u32 *lock) {
  u32 expected = 0;
  while (atomic_compare_exchange(lock, &expected, 1)) cpu_relax();
}
void release_mutex(u32 *lock) {
  volatile_store(lock, 0);
}

never_inline void do_nothing() {}
never_inline void blocking_arena_alloc(Thread t, rawptr user_data) {
  ArenaAllocator *arena = (ArenaAllocator *)(user_data);
  uptr size = 1;
  wait_for_mutex(&arena->mutex);
  byte *ptr = (byte *)(volatile_load(&arena->next));
  volatile_store(&arena->next, uptr(ptr + size));
  release_mutex(&arena->mutex);
  assert(uptr(ptr + size) < arena->end);
  *ptr = 0;
}
never_inline void starvation_free_arena_alloc(Thread t, rawptr user_data) {
  ArenaAllocator *arena = (ArenaAllocator *)(user_data);
  uptr size = 1;
  u32 ticket = wait_for_ticket_mutex(&arena->lock);
  byte *ptr = (byte *)(volatile_load(&arena->next));
  volatile_store(&arena->next, uptr(ptr + size));
  release_ticket_mutex(&arena->lock, ticket);
  assert(uptr(ptr + size) < arena->end);
  *ptr = 0;
}
never_inline void lock_free_arena_alloc(Thread t, rawptr user_data) {
  ArenaAllocator *arena = (ArenaAllocator *)(user_data);
  uptr size = 1;
  uptr next = atomic_load(&arena->next);
  byte *ptr;
  for (;;) {
    ptr = (byte *)(next + size);
    if (atomic_compare_exchange(&arena->next, &next, uptr(ptr))) break;
  }
  assert(uptr(ptr + size) < arena->end);
  *ptr = 0;
}
never_inline void wait_free_arena_alloc(Thread t, rawptr user_data) {
  ArenaAllocator *arena = (ArenaAllocator *)(user_data);
  uptr size = 1;
  byte *ptr = (byte *)(atomic_fetch_add(&arena->next, size));
  assert(uptr(ptr + size) < arena->end);
  *ptr = 0;
}

void run_tests(Thread t, u32 thread_count, ArenaAllocator *arena) {
  if (t == 0) printfln("-- % thread% --", u32, thread_count, string, thread_count > 1 ? string("s") : string(""));
  if (barrier_split_threads(t, thread_count)) {
    repeat(0, do_nothing);
    arena->next = arena->start;
    repeat(arena, blocking_arena_alloc);
    arena->next = arena->start;
    repeat(arena, starvation_free_arena_alloc);
    arena->next = arena->start;
    repeat(arena, lock_free_arena_alloc);
    arena->next = arena->start;
    repeat(arena, wait_free_arena_alloc);
  }
  barrier_join_threads(t, 0, global_threads.logical_core_count);
}
void thread_main(Thread t) {
  ArenaAllocator *arena = stack_alloc(ArenaAllocator);
  if (single_core(t)) {
    // make arena
    Bytes buffer = page_reserve(GibiByte);
    for (iptr i = 0; i < iptr(buffer.size); i += 1) {
      volatile_store(&buffer.ptr[i], 1);
    }
    *arena = (ArenaAllocator){};
    arena->start = uptr(buffer.ptr);
    arena->end = uptr(buffer.ptr + buffer.size);
  }
  barrier_scatter(t, &arena);
  // run tests
  for (u32 thread_count = global_threads.logical_core_count; thread_count > 0; thread_count /= 2) {
    run_tests(t, thread_count, arena);
  }
}
int main() {
  // init state
  _init_console();
  _init_page_fault_handler();
  // start threads and do work
  u32 thread_count = _get_logical_core_count();
  _start_threads(thread_count);
}
