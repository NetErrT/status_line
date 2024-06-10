#include "log.h"

#include <stdarg.h>
#include <stdio.h>

static char const *const messages[] = {
  [LOG_ERROR] = "ERROR",
  [LOG_WARN] = "WARN",
};

static char const *const colors[] = {
  [LOG_ERROR] = "\033[31m",
  [LOG_WARN] = "\033[33m",
};

bool log_msg(log_level_t log_level, char const *module, char const *file, int line, char const *format, ...) {
  FILE *stream = stdout;

  if (log_level == LOG_ERROR) {
    stream = stderr;
  }

#ifdef LOG_COLORS
  fprintf(stream, "%s[%s %s:%d]%s %s: ", colors[log_level], messages[log_level], file, line, "\033[0m", module);
#else
  fprintf(stream, "[%s %s:%d]%s: ", messages[log_level], file, line, module);
#endif

  va_list args;

  va_start(args, format);
  vfprintf(stream, format, args);
  fwrite("\n", sizeof(char), 1, stream);
  va_end(args);

  return true;
}
