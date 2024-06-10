#include "utils/fs.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

inline bool utils_fs_has_dir(char const *path) {
  struct stat path_stat;

  return stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode);
}

inline bool utils_fs_has_file(char const *path) {
  struct stat path_stat;

  return stat(path, &path_stat) == 0 && S_ISREG(path_stat.st_mode);
}

inline char *utils_fs_read_file(char const *file_path, int n) {
  FILE *file = fopen(file_path, "ro");

  if (file == NULL) {
    return NULL;
  }

  char *buffer = calloc(n + 1, sizeof(*buffer));

  if (buffer == NULL) {
    return NULL;
  }

  return fgets(buffer, n, file);
}
