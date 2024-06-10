#include "status_line.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#define LOG_MODULE "status-line"

#include "log.h"
#include "macros.h"
#include "module.h"

static volatile bool is_aborted = false;

static void signal_handler(int sig) {
  is_aborted = sig;
}

static int module_thread(void *param) {
  module_t *const module = param;

  int status = module->run(module);

  thrd_exit(status);
}

static void update_wmname(xcb_connection_t *connection, char const *buffer, u32 length) {
  xcb_setup_t const *setup = xcb_get_setup(connection);
  xcb_window_t const root_window = xcb_setup_roots_iterator(setup).data->root;

  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, root_window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, length,
                      buffer);
  xcb_flush(connection);
}

static usize calculate_new_length(status_line_t const *status_line) {
  usize length = 0;

  for (usize module_index = 0; module_index < status_line->modules_count; module_index++) {
    module_t *module = &status_line->modules[module_index];

    if (module->buffer == NULL) {
      continue;
    }

    length += strlen(module->buffer);
  }

  return length;
}

bool status_line_construct(status_line_t *status_line, usize modules_count) {
  status_line->connection = xcb_connect(NULL, NULL);

  if (xcb_connection_has_error(status_line->connection)) {
    log_error("Failed connect to X server");
    goto error;
  }

  status_line->abort_file_descriptor = eventfd(0, 0);

  if (status_line->abort_file_descriptor == -1) {
    log_error("Invalid file descriptor");
    goto error;
  }

  status_line->modules = malloc(modules_count * sizeof(module_t));
  status_line->modules_count = modules_count;

  if (status_line->modules == NULL) {
    log_error("Failed to allocate modules");
    goto error;
  }

  return true;

error:
  status_line_destruct(status_line);

  return false;
}

bool status_line_run(status_line_t *status_line, config_t const *config) {
  bool status = false;

  { /* handle sigint */
    struct sigaction act = {0};
    act.sa_handler = signal_handler;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGINT, &act, NULL) == -1) {
      log_error("Failed to setup signals");
      goto done;
    }
  }

  thrd_t *thread_ids = malloc(status_line->modules_count * sizeof(*thread_ids));

  if (thread_ids == NULL) {
    log_error("Failed to allocate threads");
    goto done;
  }

  for (usize module_index = 0; module_index < status_line->modules_count; module_index++) {
    config_module_t const *const config_module = &config->modules[module_index];

    module_t *module = &status_line->modules[module_index];

    if (!module_construct(module, status_line, config_module->key, config_module->config)) {
      log_error("Failed to initialize module");
      goto free_threads;
    }

    if (thrd_create(&thread_ids[module_index], module_thread, module) != thrd_success) {
      log_error("Failed to create module thread");
      goto free_threads;
    }
  }

  struct pollfd poll_file_descriptors[] = {
    {.fd = status_line->abort_file_descriptor, .events = POLLIN},
  };

  while (!is_aborted) {
    int poll_status = poll(poll_file_descriptors, countof(poll_file_descriptors), -1);

    if (poll_status < 0) {
      if (errno == EINTR) {
        continue;
      }

      log_error("poll()");
      goto free_threads;
    }

    log_error("close file descriptor writed from module");
    break;
  }

  /* send a message to exit modules */
  write(status_line->abort_file_descriptor, &(u64){1}, sizeof(u64));

  for (usize module_index = 0; module_index < status_line->modules_count; module_index += 1) {
    thrd_t thread = thread_ids[module_index];

    if (thrd_join(thread, NULL) != thrd_success) {
      log_error("Failed to close module thread");
      goto free_threads;
    }
  }

  status = true;

free_threads:
  free(thread_ids);

done:
  return status;
}

void status_line_destruct(status_line_t *status_line) {
  /* set WM_NAME to empty string */
  if (status_line->connection != NULL) {
    update_wmname(status_line->connection, NULL, 0);
    xcb_aux_sync(status_line->connection);
    xcb_disconnect(status_line->connection);
  }

  /* free modules */
  if (status_line->modules != NULL) {
    for (usize module_index = 0; module_index < status_line->modules_count; module_index++) {
      module_t *module = &status_line->modules[module_index];

      module_destruct(module);
    }

    free(status_line->modules);
  }

  if (status_line->abort_file_descriptor != -1) {
    close(status_line->abort_file_descriptor);
  }
}

void status_line_update(status_line_t const *status_line) {
  usize const buffer_length = calculate_new_length(status_line);
  char *buffer = malloc((buffer_length + 1) * sizeof(*buffer));

  if (buffer == NULL) {
    return;
  }

  for (usize module_index = 0, length = 0; module_index < status_line->modules_count; module_index++) {
    module_t *module = &status_line->modules[module_index];

    if (module->buffer == NULL) {
      continue;
    }

    usize module_buffer_length = strlen(module->buffer);

    mtx_lock(&module->lock);
    memcpy(buffer + length, module->buffer, module_buffer_length);
    mtx_unlock(&module->lock);

    length += module_buffer_length;
  }

  update_wmname(status_line->connection, buffer, buffer_length);

  free(buffer);
}
