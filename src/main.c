#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/mem.h"
#include "lib/threads.h"

// params
#define WARMUP_COUNT      0
#define REPEAT_COUNT      1000
#define REPEAT_GROUP_SIZE 1000

// timings
#define repeat(user_data, callback) repeat_impl(t, user_data, callback, string(#callback));
void repeat_impl(Thread t, rawptr user_data, void (*callback)(Thread t, rawptr user_data), string name) {
  // repeat n times
  u64 results[REPEAT_COUNT];
  for (i64 i = -WARMUP_COUNT; i < REPEAT_COUNT; i += 1) {
    u64 cycles_before = read_cycle_counter();
    for (u64 j = 0; j < REPEAT_GROUP_SIZE; j++) callback(t, user_data);
    u64 cycles_after = read_cycle_counter();
    if (i >= 0) results[i] = cycles_after - cycles_before;
  }
  // compute metrics
  u64 group_cycles_sum = 0;
  u64 group_cycles_max = 0;
  for (u64 i = 0; i < REPEAT_COUNT; i++) {
    u64 dgroup_cycles = results[i];
    group_cycles_sum += dgroup_cycles;
    if (dgroup_cycles > group_cycles_max) group_cycles_max = dgroup_cycles;
  }
  // print result
  if (single_core(t)) {
    f64 cycles_mean = f64(group_cycles_sum) / f64(REPEAT_COUNT * REPEAT_GROUP_SIZE);
    f64 cycles_max = f64(group_cycles_max) / f64(REPEAT_GROUP_SIZE);
    printfln("  %: % cy (% cy)", string, name, u64, u64(cycles_mean), u64, u64(cycles_max));
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
  TicketMutex lock;
  u32 mutex;
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
  *lock = 0;
}

void do_nothing() {}
void starvation_free_arena_alloc(Thread t, rawptr user_data) {
  ArenaAllocator *arena = (ArenaAllocator *)(user_data);
  uptr size = 1;
  u32 ticket = wait_for_ticket_mutex(&arena->lock);
  byte *ptr = (byte *)(volatile_load(&arena->next));
  volatile_store(&arena->next, uptr(ptr + size));
  release_ticket_mutex(&arena->lock, ticket);
  assert(uptr(ptr + size) < arena->end);
  *ptr = 0;
}
void lock_free_arena_alloc(Thread t, rawptr user_data) {
  ArenaAllocator *arena = (ArenaAllocator *)(user_data);
  uptr size = 1;
  wait_for_mutex(&arena->mutex);
  byte *ptr = (byte *)(volatile_load(&arena->next));
  volatile_store(&arena->next, uptr(ptr + size));
  release_mutex(&arena->mutex);
  assert(uptr(ptr + size) < arena->end);
  *ptr = 0;
}
void wait_free_arena_alloc(Thread t, rawptr user_data) {
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
    repeat(arena, wait_free_arena_alloc);
    arena->next = arena->start;
    repeat(arena, lock_free_arena_alloc);
    arena->next = arena->start;
    repeat(arena, starvation_free_arena_alloc);
  }
  barrier_join_threads(t, 0, global_threads.logical_core_count);
}
void thread_main(Thread t) {
  ArenaAllocator *arena = stack_alloc(ArenaAllocator);
  if (single_core(t)) {
    // make arena
    Bytes buffer = page_reserve(GibiByte);
    for (iptr i = 0; i < iptr(buffer.size); i += 4 * KibiByte) {
      atomic_store(&buffer.ptr[i], 0);
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
