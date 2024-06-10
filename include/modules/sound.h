#pragma once

#include "module.h"

typedef struct module_sound_config {
  char *format;  /* formats:
                    %volume% - volume level in percent
                    %state% - if muted returns M else m */
  char *control; /* alsa control (e.g "Master") */
  char *device;  /* alsa device (e.g "default", "hw:0" ) */
} module_sound_config_t;

int module_sound_run(module_t *module);
