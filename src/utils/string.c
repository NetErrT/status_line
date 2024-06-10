#include "utils/string.h"

#include <stdbool.h>
#include <string.h>

bool utils_string_replace(char *buffer, size_t max_size, char const *search, char const *replace) {
  if (search == NULL || replace == NULL || buffer == NULL) {
    return false;
  }

  size_t const search_length = strlen(search);
  size_t const replace_length = strlen(replace);

  size_t buffer_length = strlen(buffer);
  char *buffer_ptr = buffer;

  while ((buffer_ptr = strstr(buffer_ptr, search)) != NULL) {
    size_t remaining_length = strlen(buffer_ptr);

    if ((buffer_length - search_length + replace_length) > max_size) {
      return false;
    }

    memmove(buffer_ptr + replace_length, buffer_ptr + search_length, remaining_length - search_length + 1);
    memcpy(buffer_ptr, replace, replace_length);

    buffer_ptr += replace_length;
    buffer_length += replace_length - search_length;
  }

  return true;
}

size_t utils_string_replace_count(char const *buffer, char const *search) {
  if (search == NULL || buffer == NULL) {
    return false;
  }

  size_t const search_length = strlen(search);

  size_t n = 0;
  char const *buffer_ptr = buffer;

  while ((buffer_ptr = strstr(buffer_ptr, search))) {
    n += 1;
    buffer_ptr += search_length;
  }

  return n;
}
