#pragma once

#include "module.h"

typedef struct module_keyboard_config {
  char *format; /* formats:
                 %caps% - "C" if enabled otherwise "c"
                 %num% - "N" if enabled otherwise "n"
                 %scroll% - "S" if enabled otherwise "s"
                 %symbol% - short layout name (e.g "us")
                 %name% - full layout name (e.g "English (US)") */
} module_keyboard_config_t;

int module_keyboard_run(module_t *module);
