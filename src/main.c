#include <stdlib.h>

#include "config.h"
#include "log.h"
#include "status_line.h"

int main(void) {
  int status = EXIT_FAILURE;
  config_t config = {0};

  if (!config_construct(&config)) {
    log_error("Failed to get config");
    goto done;
  }

  status_line_t status_line = {0};

  if (!status_line_construct(&status_line, config.modules_count)) {
    log_error("Failed to initialize status line");
    goto free_config;
  }

  if (!status_line_run(&status_line, &config)) {
    log_error("Failed to run status line");
    goto free_status_line;
  }

  status = EXIT_SUCCESS;

free_status_line:
  status_line_destruct(&status_line);

free_config:
  config_destruct(&config);

done:
  return status;
}
