#pragma once

#define countof(array) (sizeof(array) / sizeof((array)[0]))
#define lengthof(array) (countof(array) - 1)
#define strfsize(...) snprintf(NULL, 0, __VA_ARGS__)
