#include "modules/keyboard.h"

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xkb.h>

#define LOG_MODULE "keyboard"

#include "log.h"
#include "macros.h"
#include "module.h"
#include "toml.h"

enum {
  INDICATOR_CAPSLOCK = 1,
  INDICATOR_NUMLOCK = 2,
  INDICATOR_SCROLLLOCK = 4,
};

typedef struct {
  char *name;
  char *symbol;
  bool is_capslock;
  bool is_numlock;
  bool is_scrolllock;
} private_t;

typedef enum {
  NOEVENT,
  EVENT,
  ERROR,
} handle_events_status_t;

static bool enable_xkb(xcb_connection_t *connection) {
  int status = false;

  xcb_generic_error_t *error = NULL;
  xcb_xkb_use_extension_cookie_t cookie;
  xcb_xkb_use_extension_reply_t *reply = NULL;

  uint16_t major_version = XCB_XKB_MAJOR_VERSION;
  uint16_t minor_version = XCB_XKB_MINOR_VERSION;

  cookie = xcb_xkb_use_extension(connection, major_version, minor_version);
  reply = xcb_xkb_use_extension_reply(connection, cookie, &error);

  if (reply == NULL || error != NULL) {
    log_error("Failed to query for XKB extension");
  } else if (!reply->supported) {
    log_error("XKB extension is not supported");
  } else {
    status = true;
  }

  free(reply);

  return status;
}

static bool register_events(xcb_connection_t *connection) {
  uint16_t events = XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_STATE_NOTIFY |
                    XCB_XKB_EVENT_TYPE_INDICATOR_STATE_NOTIFY;

  xcb_void_cookie_t cookie =
    xcb_xkb_select_events_checked(connection, XCB_XKB_ID_USE_CORE_KBD, events, 0, events, 0, 0, NULL);

  if (xcb_request_check(connection, cookie) != NULL) {
    log_error("Failed for register xkb events");

    return false;
  }

  return true;
}

static uint8_t get_state_group(xcb_connection_t *connection, xcb_generic_error_t *error) {
  uint8_t group = 0;

  xcb_xkb_get_state_cookie_t cookie;
  xcb_xkb_get_state_reply_t *reply;

  cookie = xcb_xkb_get_state(connection, XCB_XKB_ID_USE_CORE_KBD);
  reply = xcb_xkb_get_state_reply(connection, cookie, &error);

  if (reply != NULL && error == NULL) {
    group = reply->group;
  }

  free(reply);

  return group;
}

static uint32_t get_indicator_state(xcb_connection_t *connection, xcb_generic_error_t *error) {
  uint8_t state = 0;

  xcb_xkb_get_indicator_state_cookie_t cookie;
  xcb_xkb_get_indicator_state_reply_t *reply;

  cookie = xcb_xkb_get_indicator_state(connection, XCB_XKB_ID_USE_CORE_KBD);
  reply = xcb_xkb_get_indicator_state_reply(connection, cookie, &error);

  if (reply != NULL && error == NULL) {
    state = reply->state;
  }

  free(reply);

  return state;
}

static char *get_xcb_atom(xcb_connection_t *connection, xcb_atom_t atom) {
  char *buffer = NULL;

  xcb_generic_error_t *error;
  xcb_get_atom_name_cookie_t cookie;
  xcb_get_atom_name_reply_t *reply = NULL;

  cookie = xcb_get_atom_name(connection, atom);
  reply = xcb_get_atom_name_reply(connection, cookie, &error);

  if (reply == NULL || error != NULL) {
    goto done;
  }

  int name_length = xcb_get_atom_name_name_length(reply);
  char *name = xcb_get_atom_name_name(reply);

  buffer = malloc(name_length + 1);

  if (buffer == NULL) {
    log_error("Failed to allocate buffer for atom value");

    goto done;
  }

  memcpy(buffer, name, name_length);
  buffer[name_length] = '\0';

done:
  free(reply);

  return buffer;
}

static inline void get_private_indicators(private_t *info, uint32_t state) {
  info->is_capslock = (state & INDICATOR_CAPSLOCK) != 0;
  info->is_numlock = (state & INDICATOR_NUMLOCK) != 0;
  info->is_scrolllock = (state & INDICATOR_SCROLLLOCK) != 0;
}

static bool get_private_layout(xcb_connection_t *connection, uint8_t group, private_t *info) {
  bool status = false;

  xcb_generic_error_t *error = NULL;
  xcb_xkb_get_names_reply_t *names_reply = NULL;
  xcb_xkb_get_names_cookie_t names_cookie;
  xcb_xkb_get_names_value_list_t names_list;
  void *names_list_buffer = NULL;

  xcb_xkb_device_spec_t device_spec = XCB_XKB_ID_USE_CORE_KBD;
  uint32_t names_cookie_which = XCB_XKB_NAME_DETAIL_SYMBOLS | XCB_XKB_NAME_DETAIL_GROUP_NAMES;

  names_cookie = xcb_xkb_get_names(connection, device_spec, names_cookie_which);
  names_reply = xcb_xkb_get_names_reply(connection, names_cookie, &error);

  if (error != NULL) {
    log_error("Failed to get keyboard names(info)");

    goto done;
  }

  names_list_buffer = xcb_xkb_get_names_value_list(names_reply);

  // clang-format off
  xcb_xkb_get_names_value_list_unpack(names_list_buffer,
    names_reply->nTypes,
    names_reply->indicators,
    names_reply->virtualMods,
    names_reply->groupNames,
    names_reply->nKeys,
    names_reply->nKeyAliases,
    names_reply->nRadioGroups,
    names_reply->which,
    &names_list);
  // clang-format on

  info->name = get_xcb_atom(connection, names_list.groups[group]);

  {
    char const *delimeter = "+";
    char *symbols = get_xcb_atom(connection, names_list.symbolsName);
    char *symbol_tok_save = NULL;
    char *symbol_tok = strtok_r(symbols, delimeter, &symbol_tok_save);

    symbol_tok = strtok_r(NULL, delimeter, &symbol_tok_save);

    for (int i = 0; symbol_tok != NULL && i < group; i++) {
      symbol_tok = strtok_r(NULL, delimeter, &symbol_tok_save);
    }

    size_t const symbol_size = 2;
    char *symbol = malloc(symbol_size + 1);

    if (symbol == NULL) {
      log_error("Failed to allocate keyboard symbols (e.q \"us\")");

      goto done;
    }

    memcpy(symbol, symbol_tok, symbol_size);
    symbol[symbol_size] = '\0';

    info->symbol = symbol;

    free(symbols);
  }

  status = true;

done:
  free(names_reply);

  return status;
}

static inline void free_private_fields(private_t *private) {
  free(private->symbol);
  free(private->name);
}

static bool get_private(xcb_connection_t *connection, private_t *private) {
  xcb_generic_error_t *error = NULL;

  uint8_t state_group = get_state_group(connection, error);

  if (error != NULL) {
    log_error("Failed to get current keyboard layout group");

    return false;
  }

  free_private_fields(private);

  if (!get_private_layout(connection, state_group, private)) {
    log_error("Failed to get keyboard layout");

    return false;
  }

  uint32_t indicator_state = get_indicator_state(connection, error);

  if (error != NULL) {
    log_error("Failed to get keyboard indicators");

    return false;
  }

  get_private_indicators(private, indicator_state);

  return true;
}

static void free_config(module_keyboard_config_t *config) {
  free(config->format);
  free(config);
}

static module_keyboard_config_t *get_and_check_config(toml_table_t *table) {
  module_keyboard_config_t *config = calloc(1, sizeof(*config));

  toml_datum_t format = toml_string_in(table, "format");

  if (!format.ok) {
    log_error("Failed to get format");
    goto error;
  }

  config->format = format.u.s;

  return config;

error:
  free_config(config);
  return NULL;
}

static inline bool update(module_t *module, module_keyboard_config_t const *config, private_t const *info) {
  char const *formatters[][2] = {
    {"%caps%", !info->is_capslock ? "c" : "C"},
    {"%num%", !info->is_numlock ? "n" : "N"},
    {"%scroll%", !info->is_scrolllock ? "s" : "S"},
    {"%symbol%", info->symbol},
    {"%name%", info->name},
    {NULL, NULL},
  };

  return module_update(module, config->format, formatters);
}

static handle_events_status_t handle_events(xcb_connection_t *connection, private_t *info) {
  xcb_generic_event_t *xcb_event = NULL;
  handle_events_status_t status = NOEVENT;

  while ((xcb_event = xcb_poll_for_event(connection))) {
    switch (xcb_event->pad0) {
      case XCB_XKB_NEW_KEYBOARD_NOTIFY: {
        if (!get_private(connection, info)) {
          log_error("Failed to get keyboard layout and indicators");

          goto error;
        }

        status = EVENT;

        break;
      }
      case XCB_XKB_INDICATOR_STATE_NOTIFY: {
        xcb_xkb_indicator_state_notify_event_t const *event = NULL;
        event = (xcb_xkb_indicator_state_notify_event_t const *)xcb_event;

        static int const events = INDICATOR_CAPSLOCK | INDICATOR_NUMLOCK | INDICATOR_SCROLLLOCK;

        if (!(event->stateChanged & events)) {
          break;
        }

        get_private_indicators(info, event->state);

        status = EVENT;

        break;
      }
      case XCB_XKB_STATE_NOTIFY: {
        xcb_xkb_state_notify_event_t const *event = NULL;
        event = (xcb_xkb_state_notify_event_t const *)xcb_event;

        if (!(event->changed & XCB_XKB_STATE_PART_GROUP_STATE)) {
          break;
        }

        free_private_fields(info);

        if (!get_private_layout(connection, event->group, info)) {
          log_error("Failed to get keyboard layout");

          goto error;
        }

        status = EVENT;

        break;
      }
      default:
        break;
    }

    free(xcb_event);
  }

  return status;

error:
  free(xcb_event);

  return ERROR;
}

int module_keyboard_run(module_t *module) {
  int status = EXIT_FAILURE;

  module_keyboard_config_t *config = get_and_check_config(module->config);

  if (config == NULL) {
    goto done;
  }

  xcb_connection_t *connection = xcb_connect(NULL, NULL);

  if (xcb_connection_has_error(connection)) {
    log_error("Failed connect to server");

    goto free_config;
  }

  if (!enable_xkb(connection)) {
    goto free_connection;
  }

  if (!register_events(connection)) {
    goto free_connection;
  }

  private_t info = {0};

  if (!get_private(connection, &info)) {
    goto free_connection;
  }

  struct pollfd fds[] = {
    {.fd = module_get_abort_file_descriptor(module), .events = POLLIN},
    {.fd = xcb_get_file_descriptor(connection), .events = POLLIN | POLLHUP},
  };

  while (true) {
    if (!update(module, config, &info)) {
      log_error("Failed to update keyboard module");

      goto free_info;
    }

  poll_start:
    if (poll(fds, countof(fds), -1) < 0) {
      if (errno == EINTR) {
        continue;
      }

      log_error("Failed to poll");

      goto free_info;
    }

    if (fds[0].revents & POLLIN) {
      break;
    }

    if (fds[1].revents & POLLHUP) {
      log_error("x11 disconnected");

      goto free_info;
    }

    handle_events_status_t events_status = handle_events(connection, &info);

    if (events_status == NOEVENT) {
      goto poll_start;
    }

    if (events_status == ERROR) {
      log_error("Filed to handle events");

      goto free_info;
    }
  }

  status = EXIT_SUCCESS;

free_info:
  free_private_fields(&info);

free_connection:
  xcb_disconnect(connection);

free_config:
  free_config(config);

done:
  return status;
}
