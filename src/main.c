#include "lib/builtin.h"
#include "lib/fmt.h"
#include "lib/mem.h"
#include "lib/threads.h"

// params
#define WARMUP_COUNT      1000
#define REPEAT_COUNT      1 * Mega
#define REPEAT_GROUP_SIZE 1000
ASSERT((WARMUP_COUNT + REPEAT_COUNT) % REPEAT_GROUP_SIZE == 0);

// timings
#define repeat(user_data, callback) repeat_impl(t, user_data, callback, string(#callback));
void repeat_impl(Thread t, rawptr user_data, void (*callback)(Thread t, rawptr user_data), string name) {
  // repeat n times
  u64 cycles_count = 0;
  u64 cycles_sum = 0;
  u64 cycles_max = 0;
  for (u64 i = 0; i < WARMUP_COUNT + REPEAT_COUNT; i += REPEAT_GROUP_SIZE) {
    // time `REPEAT_GROUP_SIZE` runs
    u64 cycle_times[REPEAT_GROUP_SIZE];
    for (u64 j = 0; j < REPEAT_GROUP_SIZE; j++) {
      u64 cycles_before = read_cycle_counter();
      callback(t, user_data);
      u64 cycles_after = read_cycle_counter();
      cycle_times[j] = cycles_after - cycles_before;
    }
    // sort the times
    for (u64 j = 1; j < REPEAT_GROUP_SIZE; j++) {
      u64 k = j;
      u64 current = cycle_times[k];
      for (; k > 0; k--) {
        u64 prev = cycle_times[k - 1];
        if (prev > current) cycle_times[k] = prev;
        else break;
      }
      cycle_times[k] = current;
    }
    // compute metrics
    if (i >= WARMUP_COUNT) {
      for (u64 j = 0; j < REPEAT_GROUP_SIZE; j++) {
        u64 dcycles = cycle_times[j];
        cycles_count += 1;
        cycles_sum += dcycles;
        // store max of the lowest 50% (don't time OS interrupts)
        if (j < REPEAT_GROUP_SIZE / 2 && dcycles > cycles_max) cycles_max = dcycles;
      }
    }
  }
  // print result
  if (single_core(t)) {
    f64 cycles_mean = f64(cycles_sum) / f64(cycles_count);
    printfln("  %: % cy (% cy)", string, name, u64, u64(cycles_mean), u64, cycles_max);
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
  if (t == 0) printfln("-- % threads --", u32, thread_count);
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
