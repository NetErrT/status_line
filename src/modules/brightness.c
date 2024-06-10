#include "modules/brightness.h"

#include <alloca.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define LOG_MODULE "brightness"

#include "log.h"
#include "macros.h"
#include "module.h"
#include "toml.h"
#include "utils/fs.h"

#define BACKLIGHT_PATH "/sys/class/backlight"
#define MAX_BRIGHTNESS_LENGTH 5

typedef struct private {
  char *brightness_file_path;
  char *max_brightness_file_path;
  u8 brightness;
}
private_t;

static void private_destruct(private_t *private) {
  free(private->brightness_file_path);
  free(private->max_brightness_file_path);
}

static bool private_construct(private_t *private, char const *card) {
  static char *file_format = "%s/%s/%s";
  static char *files[] = {"brightness", "max_brightness"};

  char *paths[countof(files)] = {0};

  for (usize path_index = 0; path_index < countof(paths); path_index += 1) {
    usize path_size = strfsize(file_format, BACKLIGHT_PATH, card, files[path_index]);
    char *path = malloc(path_size + 1);

    if (path == NULL) {
      goto error;
    }

    snprintf(path, path_size + 1, file_format, BACKLIGHT_PATH, card, files[path_index]);

    paths[path_index] = path;
  }

  private->brightness_file_path = paths[0];
  private->max_brightness_file_path = paths[1];
  private->brightness = 0;

  return private;

error:
  private_destruct(private);

  return NULL;
}

static bool private_get_brightness(private_t *private) {
  char *brightness_str = utils_fs_read_file(private->brightness_file_path, MAX_BRIGHTNESS_LENGTH);
  char *max_brightness_str = utils_fs_read_file(private->max_brightness_file_path, MAX_BRIGHTNESS_LENGTH);

  if (brightness_str == NULL || max_brightness_str == NULL) {
    log_error("Failed to get brightness");
    return false;
  }

  long brightness = atol(brightness_str);
  long max_brightness = atol(max_brightness_str);

  free(brightness_str);
  free(max_brightness_str);

  private->brightness = (i8)round((double)brightness / max_brightness * 100);

  return true;
}

static void config_free(module_brightness_config_t *config) {
  free(config->card);
  free(config->format);
  free(config);
}

static module_brightness_config_t *config_get(toml_table_t *table) {
  module_brightness_config_t *config = calloc(1, sizeof(*config));

  toml_datum_t format = toml_string_in(table, "format");

  if (!format.ok) {
    log_error("Failed to get format");
    goto error;
  }

  toml_datum_t card = toml_string_in(table, "card");

  if (!card.ok) {
    log_error("Failed to get card");
    goto error;
  }

  config->format = format.u.s;
  config->card = card.u.s;

  return config;

error:
  config_free(config);
  return NULL;
}

static bool handle_events(int inotifyfd, private_t *private) {
  char buffer[sizeof(struct inotify_event)] = {0};
  isize length = 0, n = 0;
  struct inotify_event const *event = NULL;

  length = read(inotifyfd, buffer, sizeof(buffer));

  if (length < 0 && errno == EINTR) {
    return true;
  }

  if (length <= 0) {
    return false;
  }

  while (n < length) {
    event = (struct inotify_event const *)&buffer[n];

    if (event->mask & (IN_CLOSE_WRITE | IN_CREATE)) {
      if (!private_get_brightness(private)) {
        return false;
      }
    } else if (event->mask & IN_DELETE) {
      private->brightness = -1;
    }

    n += sizeof(struct inotify_event) + event->len;
  }

  return true;
}

static inline bool update_module(module_t *module, module_brightness_config_t const *config, private_t const *private) {
  char brightness_buffer[4] = {0};
  snprintf(brightness_buffer, sizeof(brightness_buffer), "%d", (u8) private->brightness);

  char const *formatters[][2] = {
    {"%value%", brightness_buffer},
    {NULL, NULL},
  };

  return module_update(module, config->format, formatters);
}

int module_brightness_run(module_t *module) {
  int status = EXIT_FAILURE;

  module_brightness_config_t *config = config_get(module->config);

  if (config == NULL) {
    goto done;
  }

  private_t private;

  if (!private_construct(&private, config->card)) {
    log_error("Failed to initialize private struct");
    goto free_config;
  }

  update_module(module, config, &private);

  if (!utils_fs_has_file(private.brightness_file_path) || !utils_fs_has_file(private.max_brightness_file_path)) {
    log_error("Failed to get brightness files");
    goto free_private;
  }

  int inotifyfd = inotify_init1(0);

  if (inotifyfd == -1) {
    log_error("Failed to initialize inotify");
    goto free_private;
  }

  if (!private_get_brightness(&private)) {
    return false;
  }

  int wd = inotify_add_watch(inotifyfd, private.brightness_file_path, IN_CLOSE_WRITE | IN_DELETE_SELF | IN_CREATE);

  if (wd == -1) {
    log_error("Failed to add inotify watch");
    goto close_inotify;
  }

  struct pollfd pfds[] = {
    {.fd = module_get_abort_file_descriptor(module), .events = POLLIN},
    {.fd = inotifyfd, .events = POLLIN},
  };

  while (true) {
    if (!update_module(module, config, &private)) {
      log_error("Failed to update module");
      goto close_wd;
    }

    if (poll(pfds, countof(pfds), -1) < 0) {
      if (errno == EINTR) {
        continue;
      }

      log_error("Failed to poll");
      goto close_wd;
    }

    if (pfds[0].revents & POLLIN) {
      break;
    }

    if (!handle_events(inotifyfd, &private)) {
      log_error("Failed to handle events");
      goto close_wd;
    }
  }

  status = EXIT_SUCCESS;

close_wd:
  close(wd);

close_inotify:
  close(inotifyfd);

free_private:
  private_destruct(&private);

free_config:
  config_free(config);

done:
  return status;
}
