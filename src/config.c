#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <toml.h>

#define LOG_MODULE "config"

#include "log.h"
#include "macros.h"
#include "typedefs.h"
#include "utils/fs.h"

static char *get_config_path(void) {
  static char const *const private[][2] = {
    /* environment name, format: environment:0, file_name:1 */
    {"XDG_CONFIG_HOME", "%s/%s"},
    {"HOME", "%s/.config/%s"},
  };
  static char const *const config_name = "status_line.toml";

  for (usize private_index = 0; private_index < countof(private); private_index++) {
    char const *environment_name = private[private_index][0];
    char const *format = private[private_index][1];

    char *environment = getenv(environment_name);

    if (environment == NULL || strlen(environment) == 0) {
      continue;
    }

    usize config_path_length = (usize)strfsize(format, environment, config_name);
    char *config_path = malloc(config_path_length + 1);

    if (config_path == NULL) {
      log_error("Failed to allocate config file path");
      return NULL;
    }

    snprintf(config_path, config_path_length + 1, format, environment, config_name);

    if (!utils_fs_has_file(config_path)) {
      continue;
    }

    return config_path;
  }

  return NULL;
}

bool config_construct(config_t *config) {
  char *config_file_path = get_config_path();

  if (config_file_path == NULL) {
    goto error;
  }

  FILE *config_file = fopen(config_file_path, "r");
  free(config_file_path);

  if (config_file == NULL) {
    log_error("Failed to open config file");
    goto error;
  }

  toml_table_t *config_root = toml_parse_file(config_file, NULL, 0);
  fclose(config_file);

  if (config_root == NULL) {
    log_error("Failed to parse config");
    goto error;
  }

  config->_private = config_root;

  toml_array_t *modules = toml_table_array(config_root, "modules");

  if (modules == NULL) {
    log_error("Failed to get modules");
  }

  int modules_size = toml_array_len(modules);
  usize modules_count = 0;

  for (int module_index = 0; module_index < modules_size; module_index++) {
    toml_table_t *module = toml_array_table(modules, module_index);

    if (module == NULL) {
      log_error("Failed to get module");
      goto error;
    }

    toml_table_t *module_config = toml_table_table(module, "config");

    if (module_config == NULL) {
      log_error("Module config is null");
      goto error;
    }

    modules_count += 1;
  }

  config->modules = calloc(modules_count, sizeof(*config->modules));

  if (config->modules == NULL) {
    log_error("Failed to allocate modules");
    goto error;
  }

  for (u8 module_index = 0; module_index < modules_count; module_index++) {
    toml_table_t *module = toml_array_table(modules, module_index);

    if (module == NULL) {
      log_error("Failed to get module");
      goto error;
    }

    toml_value_t module_key = toml_table_string(module, "name");

    if (!module_key.ok) {
      log_error("Failed to get module key");
      goto error;
    }

    toml_table_t *module_config = toml_table_table(module, "config");

    if (module_config == NULL) {
      log_error("Module config is null");
      goto error;
    }

    config->modules[module_index] = (config_module_t){.key = module_key.u.s, .config = module_config};
  }

  config->modules_count = modules_count;

  return true;

error:
  toml_free(config->_private);
  return false;
}

void config_destruct(config_t *config) {
  for (u8 module_index = 0; module_index < config->modules_count; module_index++) {
    config_module_t *module = &config->modules[module_index];

    if (module->key == NULL) {
      continue;
    }

    free(module->key);
  }

  free(config->modules);
  toml_free(config->_private);
}
