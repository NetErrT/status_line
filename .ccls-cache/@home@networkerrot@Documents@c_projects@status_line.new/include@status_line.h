#pragma once

#include <xcb/xcb.h>

#include "module.h"

typedef struct status_line {
  int abort_file_descriptor;
  xcb_connection_t *connection;
  module_t **modules;
} status_line_t;
