#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "minibook_x.h"
#include "../debug.h"

typedef struct minibook_x_s {
  laptop_device_t device;
  float accel_screen_scale;
  float accel_base_scale;
} minibook_x;

__attribute__((noinline))
static bool read_screen_accel(const laptop_device_t *self, accel_state_t *state, char **error) {
  if (!iio_device_accel_read_raw_state(0, state, error)) {
    return false;
  }
  accel_state_apply_scale(state, ((minibook_x*)self)->accel_screen_scale);
  return true;
}

__attribute__((noinline))
static bool read_base_accel(const laptop_device_t *self, accel_state_t *state, char **error) {
  if (!iio_device_accel_read_raw_state(1, state, error)) {
    return false;
  }
  accel_state_apply_scale(state, ((minibook_x*)self)->accel_base_scale);
  return true;
}

__attribute__((noinline))
static void destroy(struct laptop_device_s *self) {
  free(self);
}

__attribute__((noinline))
static bool is_current_device(const char* model, size_t model_len) {
  return model_len >= 10 && strncmp(model, "MiniBook X", 10) == 0;
}

__attribute__((noinline))
static bool create(laptop_device_t **device, char **error) {
  // Check that we have screen accelerometeer
  if (!iio_device_is_available(0)) {
    make_error(error, "Cannot find the screen accelerometer: id = 0");
    return false;
  }
  // Check that base accelerometer isn't enabled
  if (!iio_device_is_available(1)) {
    uint8_t i2c = 0;
    if (!iio_device_get_i2c_port(0, &i2c, error)) {
      return false;
    }
    if (i2c > 0) {
      i2c--;
    }
    char buffer[256];
    // Enable the accelerometer
    // echo mxc4005 0x15 > /sys/bus/i2c/devices/i2c-0/new_device
    sprintf(buffer, "/sys/bus/i2c/devices/i2c-%d/new_device", (int)i2c);
    FILE *fp = fopen(buffer, "w");
    if (fp == NULL) {
      make_errorf(error, "Cannot open the %s", buffer);
      return false;
    }
    fprintf(fp, "mxc4005 0x15\n");
    fclose(fp);
    sleep(1); // Wait for the device
    if (!iio_device_is_available(1)) {
      make_errorf(error, "Cannot enable the base accelerometer: id = 1, i2c = %d", (int)i2c);
      return false;
    }
  }
  // Accelerometers enabled
  float screen_scale = 0.0f;
  float base_scale = 0.0f;
  if (!(iio_device_accel_read_scale(0, &screen_scale, error) && iio_device_accel_read_scale(1, &base_scale, error))) {
    return false;
  }
  minibook_x *mdevice = (minibook_x*)malloc(sizeof(minibook_x));
  mdevice->device.read_screen_accel = &read_screen_accel;
  mdevice->device.read_base_accel = &read_base_accel;
  mdevice->device.destroy = &destroy;
  mdevice->accel_screen_scale = screen_scale;
  mdevice->accel_base_scale = base_scale;
  *device = (laptop_device_t *)mdevice;
  return true;
}

const laptop_device_factory_t device_minibook_x = {
  .is_current_device = &is_current_device,
  .create = &create
};
