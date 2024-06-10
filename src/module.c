#include "module.h"

#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "module"

#include "log.h"
#include "macros.h"
#include "modules/brightness.h"
#include "modules/clock.h"
#include "modules/keyboard.h"
#include "modules/sound.h"
#include "status_line.h"
#include "toml.h"
#include "utils/string.h"

static bool allocate_and_copy_buffer(module_t *module, char const *format, size_t length) {
  module->buffer = malloc(length + 1);

  if (module->buffer == NULL) {
    return false;
  }

  strncpy(module->buffer, format, length);
  module->buffer[length] = '\0';

  return true;
}

static size_t calculate_new_length(char const *format, char const *formatters[][2]) {
  size_t length = strlen(format);

  for (char const *(*formatter_ptr)[2] = formatters; (*formatter_ptr)[0] && (*formatter_ptr)[1]; formatter_ptr++) {
    size_t search_count = utils_string_replace_count(format, (*formatter_ptr)[0]);
    size_t search_length = strlen((*formatter_ptr)[0]);
    size_t replace_length = strlen((*formatter_ptr)[1]);

    if (search_length <= replace_length) {
      length += (search_count * replace_length) - (search_count * search_length);
    }
  }

  return length;
}

static bool replace_formatters(char *buffer, size_t length, char const *formatters[][2]) {
  for (char const *(*formatter_ptr)[2] = formatters; (*formatter_ptr)[0] && (*formatter_ptr)[1]; formatter_ptr++) {
    if (!utils_string_replace(buffer, length, (*formatter_ptr)[0], (*formatter_ptr)[1])) {
      return false;
    }
  }

  return true;
}

bool module_update(module_t *module, char const *format, char const *formatters[][2]) {
  free(module->buffer);

  mtx_lock(&module->lock);

  if (formatters) {
    size_t length = calculate_new_length(format, formatters);

    if (!allocate_and_copy_buffer(module, format, length)) {
      log_error("Failed to allocate buffer");
      goto error;
    }

    if (!replace_formatters(module->buffer, length, formatters)) {
      log_error("Failed to replace strings");
      goto error;
    }
  } else {
    if (!allocate_and_copy_buffer(module, format, strlen(format))) {
      log_error("Failed to allocate buffer");
      goto error;
    }
  }

  mtx_unlock(&module->lock);

  status_line_update(module->status_line);

  return true;

error:
  mtx_unlock(&module->lock);

  free(module->buffer);

  return false;
}

bool module_construct(module_t *module, status_line_t *status_line, char const *key, toml_table_t *config) {
  module->status_line = status_line;
  module->config = config;
  if ((module->run = module_get_run_function(key)) == NULL) {
    return false;
  }
  module->buffer = NULL;

  if (mtx_init(&module->lock, mtx_plain) != thrd_success) {
    return false;
  }

  return true;
}

void module_destruct(module_t *module) {
  free(module->buffer);
  mtx_destroy(&module->lock);
}

typedef struct module_get_run_item {
  char *key;
  module_run run;
} module_get_run_item_t;

module_run module_get_run_function(char const *key) {
  module_get_run_item_t items[] = {
    {"clock", module_clock_run},
    {"brightness", module_brightness_run},
    {"sound", module_sound_run},
    {"keyboard", module_keyboard_run},
  };

  for (size_t item_index = 0; item_index < countof(items); item_index++) {
    if (strcmp(items[item_index].key, key) == 0) {
      return items[item_index].run;
    }
  }

  return NULL;
}

inline int module_get_abort_file_descriptor(module_t const *module) {
  return module->status_line != NULL ? module->status_line->abort_file_descriptor : -1;
}
