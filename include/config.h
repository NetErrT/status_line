#pragma once

#include <stdbool.h>

#include "toml.h"
#include "typedefs.h"

typedef struct config_module {
  char *key;
  toml_table_t *config;
} config_module_t;

typedef struct config {
  config_module_t *modules; /* array modules */
  usize modules_count;
  toml_table_t *_private;
} config_t;

bool config_construct(config_t *config);
void config_destruct(config_t *config);
