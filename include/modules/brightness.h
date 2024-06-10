#pragma once

#include "module.h"

typedef struct module_brightness_config {
  char *format; /* formats:
                   %value% - brightness level in percent */
  char *card;   /* card on path "/sys/class/backlight/" (e.g "intel_backlight") */
} module_brightness_config_t;

int module_brightness_run(module_t *module);
