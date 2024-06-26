#include "modules/clock.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#define LOG_MODULE "clock"

#include "log.h"
#include "module.h"
#include "toml.h"
#include "utils/time.h"

#define MAX_DATE_LENGTH 256

static void config_free(module_clock_config_t *config) {
  free(config->format);
  free(config);
}

static module_clock_config_t *config_get(toml_table_t *table) {
  module_clock_config_t *config = calloc(1, sizeof(*config));

  if (config == NULL) {
    log_error("Failed to allocate lock config");
    goto error;
  }

  toml_value_t format = toml_table_string(table, "format");

  if (!format.ok) {
    log_error("Failed to get lock format");
    return NULL;
  }

  config->format = format.u.s;

  toml_value_t interval = toml_table_int(table, "interval");

  if (!interval.ok) {
    log_error("Failed to get lock interval");
    goto error;
  }

  if (interval.u.i < 0) {
    log_error("Clock interval must be greater than 0");
    goto error;
  }

  config->interval = (u16)interval.u.i;

  return config;

error:
  config_free(config);
  return NULL;
}

static inline struct timeval calculate_time_until_next_interval(long interval) {
  long const current_time_ms = utils_time_get_milliseconds_since_epoch();
  long const remaining_time_ms = interval - (current_time_ms % interval);
  long const remaining_time_us = (remaining_time_ms % 1000) * 1000;

  return (struct timeval){.tv_sec = remaining_time_ms / 1000, .tv_usec = remaining_time_us};
}

static inline bool get_time_and_date(char *buffer, usize length, char const *format) {
  struct timespec current_time = {0};
  clock_gettime(CLOCK_REALTIME, &current_time);

  time_t timer = current_time.tv_sec;

  struct tm local_time;

  if (localtime_r(&timer, &local_time) == NULL) {
    return false;
  }

  strftime(buffer, length, format, &local_time);

  return true;
}

static inline bool update_module(module_t *module, module_clock_config_t const *config) {
  char buffer[MAX_DATE_LENGTH] = {0};

  if (!get_time_and_date(buffer, sizeof(buffer), config->format)) {
    return false;
  }

  return module_update(module, buffer, NULL);
}

int module_clock_run(module_t *module) {
  int status = EXIT_FAILURE;

  module_clock_config_t *config = config_get(module->config);

  if (config == NULL) {
    goto done;
  }

  tzset();

  locale_t locale = newlocale(LC_TIME_MASK, "", NULL);

  if (locale == NULL) {
    log_error("Failed to create locale");
    goto free_config;
  }

  if (uselocale(locale) == NULL) {
    log_error("Failed to use locale");
    goto free_locale;
  }

  update_module(module, config);

  long const config_interval_ms = config->interval * 1000;

  if (config_interval_ms > LONG_MAX) {
    log_error("interval int overflow");
    goto free_locale;
  }

  int abort_file_descriptor = module_get_abort_file_descriptor(module);

  if (abort_file_descriptor == -1) {
    log_error("Failed to get abort file descriptor");
    goto free_locale;
  }

  while (true) {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(abort_file_descriptor, &fds);

    struct timeval timeout = calculate_time_until_next_interval(config_interval_ms);

    int select_status = select(abort_file_descriptor + 1, &fds, NULL, NULL, &timeout);

    if (select_status < 0) {
      if (errno == EINTR) {
        continue;
      }

      log_error("select()");
      goto free_locale;
    }

    if (select_status > 0) {
      break;
    }

    if (!update_module(module, config)) {
      log_error("Failed to update lock module");
      goto free_locale;
    }
  }

  status = EXIT_SUCCESS;

free_locale:
  freelocale(locale);

free_config:
  config_free(config);

done:

  return status;
}
