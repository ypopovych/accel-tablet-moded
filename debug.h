#pragma once

#include <stdbool.h>

bool is_debug_mode_enabled(void);
void set_debug_mode_enabled(bool is_enabled);

void debug(const char *fmt, ...);
