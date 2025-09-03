#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "minibook_8.h"
#include "../debug.h"

typedef struct minibook8_s {
    laptop_device_t device;
    accel_device_t screen;
    accel_device_t base;
} minibook8_t;

__attribute__((noinline))
static bool read_screen_accel(const laptop_device_t *self, accel_state_t *state, char **error) {
    return iio_device_accel_read_state(&((minibook8_t*)self)->screen, state, error);
}

__attribute__((noinline))
static bool read_base_accel(const laptop_device_t *self, accel_state_t *state, char **error) {
    return iio_device_accel_read_state(&((minibook8_t*)self)->base, state, error);
}

__attribute__((noinline))
static void destroy(struct laptop_device_s *self) {
    iio_device_accel_close(&((minibook8_t*)self)->screen);
    iio_device_accel_close(&((minibook8_t*)self)->base);
    free((minibook8_t*)self);
}

__attribute__((noinline))
static bool is_current_device(const char* model, size_t model_len) {
    debug("Checking if the device is a MiniBook 8\n");
    return model_len >= 9 && strncmp(model, "MiniBook\n", 9) == 0;
}

__attribute__((noinline))
static bool create(laptop_device_t **device, char **error) {
    debug("Creating the MiniBook 8 device\n");
    // Check that we have screen accelerometeer
    if (!iio_device_is_available(0)) {
      make_error(error, "Cannot find the screen accelerometer: id = 0");
      return false;
    }
    // Check that base accelerometer isn't enabled
    if (!iio_device_is_available(1)) {
        // Try to enable the base accelerometer
        // Reload the i2c driver
        // rmmod bmc150_accel_i2c
        // modprobe bmc150_accel_i2c
        debug("Enabling the base accelerometer\n");
        debug("Unloading the bmc150_accel_i2c\n");
        if (system("rmmod bmc150_accel_i2c") == -1) {
            make_errorf(error, "Cannot unload the bmc150_accel_i2c: %s", strerror(errno));
            return false;
        }
        debug("Loading the bmc150_accel_i2c\n");
        if (system("modprobe bmc150_accel_i2c") == -1) {
            make_errorf(error, "Cannot load the bmc150_accel_i2c: %s", strerror(errno));
            return false;
        }
        sleep(1); // Wait for the device
        debug("Checking if the base accelerometer is enabled\n");
        if (!iio_device_is_available(1)) {
            make_error(error, "Cannot enable the base accelerometer: id = 1");
            return false;
        }
    }
    debug("Base accelerometer is enabled\n");
    // Accelerometers enabled
    minibook8_t *mdevice = (minibook8_t*)malloc(sizeof(minibook8_t));
    if (!(iio_device_accel_open(0, &mdevice->screen, error) && iio_device_accel_open(1, &mdevice->base, error))) {
        return false;
    }
    mdevice->device.read_screen_accel = &read_screen_accel;
    mdevice->device.read_base_accel = &read_base_accel;
    mdevice->device.destroy = &destroy;
    *device = (laptop_device_t *)mdevice;
    return true;
}

const laptop_device_factory_t device_minibook_8 = {
    .is_current_device = &is_current_device,
    .create = &create
};
