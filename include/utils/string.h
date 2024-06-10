#pragma once

#include <stdbool.h>
#include <stddef.h>

bool utils_string_replace(char *buffer, size_t max_size, char const *search, char const *replace);
size_t utils_string_replace_count(char const *buffer, char const *search);
