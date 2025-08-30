#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define make_error(error, str)             \
do {                                       \
  size_t len = strlen(str)+1;              \
  *error = (char *)malloc(len);            \
  strncpy(*error, str, len);               \
} while (0)

#define make_errorf(error, fmt, ...)       \
do {                                       \
  *error = (char *)malloc(2048);           \
  snprintf(*error, 2048, fmt, __VA_ARGS__);\
} while (0)

struct accel_state_s {
  double x;
  double y;
  double z;
};

typedef struct accel_state_s accel_state_t;

struct laptop_device_s {
  bool (*read_screen_accel)(const struct laptop_device_s *self, accel_state_t *state, char **error);
  bool (*read_base_accel)(const struct laptop_device_s *self, accel_state_t *state, char **error);
  void (*destroy)(struct laptop_device_s *self);
};

typedef struct laptop_device_s laptop_device_t;

struct laptop_device_factory_s {
  bool (*is_current_device)(const char* model);
  bool (*create)(laptop_device_t **device, char **error);
};

typedef struct laptop_device_factory_s laptop_device_factory_t;

static inline void accel_state_apply_scale(accel_state_t *state, float scale) {
  state->x *= (double)scale;
  state->y *= (double)scale;
  state->z *= (double)scale;
}

static inline double accel_state_get_xz_angle(const accel_state_t *state) {
  return -atan2(state->x, state->z) * 180.0 / M_PI;
}

bool iio_device_available(uint8_t device_id);
bool iio_device_get_i2c_port(uint8_t device_id, uint8_t *port);
bool iio_read_accel_scale(uint8_t device_id, float *scale, char **error);
bool iio_read_accel_raw_state(uint8_t device_id, accel_state_t *state, char **error);
int laptop_device_get_model(char **model);
int input_get_event_path_by_name(const char *name, char **path);
