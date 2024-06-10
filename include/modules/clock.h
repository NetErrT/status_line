#pragma once

#include "module.h"

typedef struct module_clock_config {
  char
    *format; /* formats defined in https://www.gnu.org/software/libc/manual/html_node/Formatting-Calendar-Time.html */
  long interval; /* non-zero interval between updates in seconds */
} module_clock_config_t;

int module_clock_run(module_t *module);
