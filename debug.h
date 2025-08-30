#pragma once

#include <stdbool.h>
#include <string.h>

#define MAX_ERROR_STR_SIZE 2048

#define make_error(error, str)             \
do {                                       \
  size_t len = strlen(str)+1;              \
  *error = (char *)malloc(len);            \
  strncpy(*error, str, len);               \
} while (0)

#define make_errorf(error, fmt, ...)                        \
do {                                                        \
  *error = (char *)malloc(MAX_ERROR_STR_SIZE+1);            \
  snprintf(*error, MAX_ERROR_STR_SIZE+1, fmt, __VA_ARGS__); \
  *error[MAX_ERROR_STR_SIZE] = '\0';                         \
} while (0)

bool is_debug_mode_enabled(void);
void set_debug_mode_enabled(bool is_enabled);

void debug(const char *fmt, ...);
