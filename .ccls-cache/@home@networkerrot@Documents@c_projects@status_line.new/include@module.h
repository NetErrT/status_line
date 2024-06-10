#pragma once

#include <threads.h>

#include "status_line.h"

typedef struct module {
  status_line_t *status_line;
  char *buffer;
  void *config;
  mtx_t lock;
} module_t;
