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

typedef struct {
  char *brightness_file_path;
  char *max_brightness_file_path;
  int8_t brightness;
} info_t;

static void info_free(info_t *info) {
  free(info->brightness_file_path);
  free(info->max_brightness_file_path);
}

static bool info_init(info_t *info, char const *card) {
  if (info == NULL) {
    return NULL;
  }

  static char const *const files[] = {"brightness", "max_brightness"};

  size_t const card_length = strlen(card);

  char **const paths = alloca(countof(files) * sizeof(*paths));

  if (paths == NULL) {
    goto error;
  }

  for (size_t i = 0; i < countof(files); i += 1) {
    size_t size = countof(BACKLIGHT_PATH) + card_length + 1 + strlen(files[i]) + 1;
    char *buffer = malloc(size);

    if (buffer == NULL) {
      goto done;
    }

    snprintf(buffer, size, "%s/%s/%s", BACKLIGHT_PATH, card, files[i]);

    paths[i] = buffer;
  }

  info->brightness_file_path = paths[0];
  info->max_brightness_file_path = paths[1];
  info->brightness = 0;

done:
  return info;

error:
  info_free(info);

  return NULL;
}

static bool info_get_brightness(info_t *info) {
  char *brightness_str = utils_fs_read_file(info->brightness_file_path, MAX_BRIGHTNESS_LENGTH);
  char *max_brightness_str = utils_fs_read_file(info->max_brightness_file_path, MAX_BRIGHTNESS_LENGTH);

  if (brightness_str == NULL || max_brightness_str == NULL) {
    log_error("Failed to get brightness");
    return false;
  }

  long brightness = atol(brightness_str);
  long max_brightness = atol(max_brightness_str);

  free(brightness_str);
  free(max_brightness_str);

  info->brightness = round((double)brightness / max_brightness * 100);

  return true;
}

static bool handle_events(int inotifyfd, info_t *info) {
  char buffer[sizeof(struct inotify_event)] = {0};
  ssize_t length = 0, n = 0;
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
      if (!info_get_brightness(info)) {
        return false;
      }
    } else if (event->mask & IN_DELETE) {
      info->brightness = -1;
    }

    n += sizeof(struct inotify_event) + event->len;
  }

  return true;
}

static inline bool update(module_t *module, module_brightness_config_t const *config, info_t const *info) {
  char brightness_buffer[4] = {0};
  snprintf(brightness_buffer, sizeof(brightness_buffer), "%d", (uint8_t)info->brightness);

  char const *formatters[][2] = {
    {"%value%", brightness_buffer},
    {NULL, NULL},
  };

  return module_update(module, config->format, formatters);
}

static void free_config(module_brightness_config_t *config) {
  free(config->card);
  free(config->format);
  free(config);
}

static module_brightness_config_t *get_and_check_config(toml_table_t *table) {
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
  free_config(config);
  return NULL;
}

int module_brightness_run(module_t *module) {
  int status = EXIT_FAILURE;

  module_brightness_config_t *config = get_and_check_config(module->config);

  if (config == NULL) {
    goto done;
  }

  info_t info;

  if (!info_init(&info, config->card)) {
    log_error("Failed to initialize private info");

    goto free_config;
  }

  update(module, config, &info);

  if (!utils_fs_has_file(info.brightness_file_path) || !utils_fs_has_file(info.max_brightness_file_path)) {
    fprintf(stderr, "Failed to get brightness files %s/%s\n", BACKLIGHT_PATH, config->card);

    goto free_info;
  }

  int inotifyfd = inotify_init1(0);

  if (inotifyfd == -1) {
    log_error("Failed to initialize inotify");

    goto free_info;
  }

  if (!info_get_brightness(&info)) {
    return false;
  }

  int wd = inotify_add_watch(inotifyfd, info.brightness_file_path, IN_CLOSE_WRITE | IN_DELETE_SELF | IN_CREATE);

  if (wd == -1) {
    log_error("Failed to add inotify watch");

    goto close_inotify;
  }

  struct pollfd pfds[] = {
    {.fd = module_get_abort_file_descriptor(module), .events = POLLIN},
    {.fd = inotifyfd, .events = POLLIN},
  };

  while (true) {
    if (!update(module, config, &info)) {
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

    if (!handle_events(inotifyfd, &info)) {
      goto close_wd;
    }
  }

  status = EXIT_SUCCESS;

close_wd:
  close(wd);

close_inotify:
  close(inotifyfd);

free_info:
  info_free(&info);

free_config:
  free_config(config);

done:
  return status;
}
