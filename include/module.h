#pragma once

#include <stdbool.h>
#include <threads.h>

#include "status_line.h"
#include "toml.h"

typedef int (*module_run)(struct module *module);

typedef struct module {
  struct status_line *status_line;
  char *buffer;
  toml_table_t *config;
  module_run run;
  mtx_t lock;
} module_t;

bool module_construct(module_t *module, status_line_t *status_line, char const *key, toml_table_t *config);
void module_destruct(module_t *module);
bool module_update(module_t *module, char const *format, char const *formatters[][2]);
module_run module_get_run_function(char const *key);
int module_get_abort_file_descriptor(module_t const *module);
