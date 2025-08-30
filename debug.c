#include <stdio.h>
#include <stdarg.h>
#include "debug.h"

static volatile bool G_is_debug = false;

bool is_debug_mode_enabled(void) {
  return G_is_debug;
}

void set_debug_mode_enabled(bool is_enabled) {
  G_is_debug = is_enabled;
}

void debug(const char *fmt, ...) {
    if (G_is_debug) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

