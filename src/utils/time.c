#include "utils/time.h"

#include <time.h>

inline long utils_time_get_milliseconds_since_epoch(void) {
  struct timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);

  return current_time.tv_sec * 1000L + current_time.tv_nsec / 1000000L;
}
