#pragma once

#include <xcb/xcb.h>

#include "config.h"
#include "typedefs.h"

typedef struct status_line {
  int abort_file_descriptor;
  xcb_connection_t *connection;
  struct module *modules;
  usize modules_count;
} status_line_t;

bool status_line_construct(status_line_t *status_line, usize modules_count);
void status_line_destruct(status_line_t *status_line);
bool status_line_run(status_line_t *status_line, config_t const *config);
void status_line_update(status_line_t const *status_line);
