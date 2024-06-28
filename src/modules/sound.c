
#include "modules/sound.h"

#include <alloca.h>
#include <alsa/asoundlib.h>
#include <math.h>

#define LOG_MODULE "sound"

#include "log.h"
#include "macros.h"
#include "module.h"
#include "toml.h"

typedef struct private {
  long min;
  long max;
  long volume;
  int switch_state;
}
private_t;

static bool private_get(private_t *private, snd_mixer_selem_id_t const *id,
                        snd_mixer_selem_channel_id_t const channel_id, snd_mixer_t *mixer) {
  snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer, id);

  if (elem == NULL) {
    return false;
  }

  if (snd_mixer_selem_has_playback_channel(elem, channel_id)) {
    if (snd_mixer_selem_get_playback_volume(elem, channel_id, &private->volume) < 0 ||
        snd_mixer_selem_get_playback_switch(elem, channel_id, &private->switch_state) < 0 ||
        snd_mixer_selem_get_playback_volume_range(elem, &private->min, &private->max) < 0) {
      return false;
    }
  } else if (snd_mixer_selem_has_capture_channel(elem, channel_id)) {
    if (snd_mixer_selem_get_capture_volume(elem, channel_id, &private->volume) < 0 ||
        snd_mixer_selem_get_capture_switch(elem, channel_id, &private->switch_state) < 0 ||
        snd_mixer_selem_get_capture_volume_range(elem, &private->min, &private->max) < 0) {
      return false;
    }
  }

  return true;
}

static u8 convert_percentage(long value, long min, long max) {
  long range = max - min;

  if (range == 0) {
    return (u8)min;
  }

  return (u8)rintf((float)value / (float)range * 100);
}

static inline bool update(module_t *module, module_sound_config_t const *config, private_t const *private) {
  u8 volume = convert_percentage(private->volume, private->min, private->max);
  usize volume_string_size = (usize)strfsize("%d", volume);
  char *volume_string = alloca(volume_string_size + 1);
  snprintf(volume_string, volume_string_size + 1, "%d", volume);

  char const *formatters[][2] = {
    {"%volume%", volume_string},
    {"%state%", private->switch_state ? "m" : "M"},
    {0},
  };

  return module_update(module, config->format, formatters);
}

static void free_config(module_sound_config_t *config) {
  free(config->control);
  free(config->device);
  free(config->format);
  free(config);
}

static module_sound_config_t *get_and_check_config(toml_table_t *table) {
  module_sound_config_t *config = calloc(1, sizeof(*config));

  toml_value_t format = toml_table_string(table, "format");

  if (!format.ok) {
    goto error;
  }

  config->format = format.u.s;

  toml_value_t device = toml_table_string(table, "device");

  if (!device.ok) {
    goto error;
  }

  config->device = device.u.s;

  toml_value_t control = toml_table_string(table, "control");

  if (!control.ok) {
    goto error;
  }

  config->control = control.u.s;

  return config;

error:
  free_config(config);
  return NULL;
}

int module_sound_run(module_t *module) {
  int status = EXIT_FAILURE;

  module_sound_config_t *config = get_and_check_config(module->config);

  if (config == NULL) {
    log_error("Failed to get config");
    goto done;
  }

  if (config == NULL) {
    log_error("incorrect configuration of the audio module");
    goto free_config;
  }

  snd_mixer_t *mixer = NULL;

  if (snd_mixer_open(&mixer, 0) < 0) {
    log_error("Failed to open mixer");
    goto free_config;
  }

  if (snd_mixer_attach(mixer, config->device) < 0) {
    log_error("Failed to attach mixer");
    goto free_mixer;
  }

  if (snd_mixer_selem_register(mixer, NULL, NULL) < 0) {
    log_error("Failed to register selem");
    goto free_mixer;
  }

  if (snd_mixer_load(mixer) < 0) {
    log_error("Failed to load mixer");
    goto free_mixer;
  }

  snd_mixer_selem_id_t *id = NULL;

  snd_mixer_selem_id_alloca(&id);
  snd_mixer_selem_id_set_name(id, config->control);
  snd_mixer_selem_id_set_index(id, 0);

  int nfds = snd_mixer_poll_descriptors_count(mixer) + 1;
  struct pollfd *pfds = malloc((unsigned long)nfds * sizeof(*pfds));

  if (pfds == NULL) {
    log_error("Failed to allocate poll descriptors");
    goto free_mixer;
  }

  pfds[0].fd = module_get_abort_file_descriptor(module);
  pfds[0].events = POLLIN;

  if (snd_mixer_poll_descriptors(mixer, &pfds[1], (unsigned int)nfds - 1) < 0) {
    log_error("cannot get poll descriptors");
    goto free_pfds;
  }

  private_t private = {0};

  while (true) {
    if (!private_get(&private, id, SND_MIXER_SCHN_MONO, mixer)) {
      log_error("Failed to get channel info");
      goto free_pfds;
    }

    if (!update(module, config, &private)) {
      log_error("Failed to update status line");
      goto free_pfds;
    }

    int poll_status = poll(pfds, (nfds_t)nfds, -1);

    if (poll_status < 0) {
      if (errno == EINTR) {
        continue;
      }

      log_error("Failed to poll()");
      goto free_pfds;
    }

    if (pfds[0].revents & POLLIN) {
      break;
    }

    unsigned short revents;

    if (snd_mixer_poll_descriptors_revents(mixer, &pfds[1], (unsigned int)nfds - 1, &revents) < 0) {
      log_error("cannot get poll descriptors events");
      goto free_pfds;
    }

    if (revents & POLLIN) {
      snd_mixer_handle_events(mixer);
    } else if (revents & (POLLERR | POLLNVAL)) {
      log_error("alsa I/O error");
      goto free_pfds;
    }
  }

  status = EXIT_SUCCESS;

free_pfds:
  free(pfds);

free_mixer:
  snd_mixer_close(mixer);

free_config:
  free_config(config);

done:
  return status;
}
