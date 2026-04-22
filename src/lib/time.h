#include "builtin.h"

#define read_cycle_counter() __builtin_readcyclecounter()
// #define read_steady_counter() __builtin_readsteadycounter()

#if OS_WINDOWS
foreign void QueryPerformanceCounter(i64 *ticks);
foreign void QueryPerformanceFrequency(i64 *frequency);
#endif
i64 time_frequency() {
  static i64 frequency = 0;
  if (frequency == 0) QueryPerformanceFrequency(&frequency);
  return frequency;
}
i64 time_ns() {
#if OS_WINDOWS
  i64 ticks;
  QueryPerformanceCounter(&ticks);
  return ticks * (i64(Giga) / time_frequency());
#else
  assert(false);
#endif
}
u64 tsc_frequency() {
  static u64 frequency = 0;
  if (frequency == 0) {
    i64 ns_before = time_ns();
    i64 tsc_before = i64(read_cycle_counter());
    while (ns_before + Mega > time_ns()) {}
    i64 ns_after = time_ns();
    i64 tsc_after = i64(read_cycle_counter());
    frequency = u64((tsc_after - tsc_before) * Giga / (ns_after - ns_before));
  }
  return frequency;
}
