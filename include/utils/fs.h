#pragma once

#include <stdbool.h>

bool utils_fs_has_dir(char const *path);
bool utils_fs_has_file(char const *path);
char *utils_fs_read_file(char const *file_path, int nbytes);
