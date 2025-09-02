#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

struct accel_state_s {
    double x;
    double y;
    double z;
};

typedef struct accel_state_s accel_state_t;

struct accel_device_s {
    int fd_x;
    int fd_y;
    int fd_z;
    double scale;
};

typedef struct accel_device_s accel_device_t;

struct laptop_device_s {
    bool (*read_screen_accel)(const struct laptop_device_s *self, accel_state_t *state, char **error);
    bool (*read_base_accel)(const struct laptop_device_s *self, accel_state_t *state, char **error);
    void (*destroy)(struct laptop_device_s *self);
};

typedef struct laptop_device_s laptop_device_t;

struct laptop_device_factory_s {
    bool (*is_current_device)(const char* model, size_t model_len);
    bool (*create)(laptop_device_t **device, char **error);
};

typedef struct laptop_device_factory_s laptop_device_factory_t;

static inline void accel_state_apply_scale(accel_state_t *state, double scale) {
    state->x *= scale;
    state->y *= scale;
    state->z *= scale;
}

static inline double accel_state_get_xz_angle(const accel_state_t *state) {
    return -atan2(state->x, state->z) * 180.0 / M_PI;
}

bool laptop_device_get_model(char **model, char **error);

bool iio_device_is_available(uint8_t device_id);
bool iio_device_get_i2c_port(uint8_t device_id, uint8_t *port, char **error);
bool iio_device_accel_read_scale(uint8_t device_id, double *scale, char **error);

bool iio_device_accel_open(uint8_t device_id, accel_device_t *device, char** error);
bool iio_device_accel_read_state(const accel_device_t *device, accel_state_t *state, char **error);
void iio_device_accel_close(accel_device_t *device);

