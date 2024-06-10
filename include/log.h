#ifndef LOG_H
#define LOG_H

#include <stdbool.h>

#define LOG_COLORS

#ifndef LOG_MODULE
#define LOG_MODULE ""
#endif /* ifndef LOG_MODULE */

typedef enum {
  LOG_ERROR = 0,
  LOG_WARN,
} log_level_t;

bool log_msg(log_level_t log_level, char const* module, char const* file, int line, char const* format, ...);

#define log_error(...) log_msg(LOG_ERROR, LOG_MODULE, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_msg(LOG_WARN, LOG_MODULE, __FILE__, __LINE__, __VA_ARGS__)

#endif /* end of include guard: LOG_H */
