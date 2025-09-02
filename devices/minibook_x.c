#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "minibook_x.h"
#include "../debug.h"

typedef struct minibookx_s {
    laptop_device_t device;
    accel_device_t screen;
    accel_device_t base;
} minibookx_t;

__attribute__((noinline))
static bool read_screen_accel(const laptop_device_t *self, accel_state_t *state, char **error) {
    return iio_device_accel_read_state(&((minibookx_t*)self)->screen, state, error);
}

__attribute__((noinline))
static bool read_base_accel(const laptop_device_t *self, accel_state_t *state, char **error) {
    return iio_device_accel_read_state(&((minibookx_t*)self)->base, state, error);
}

__attribute__((noinline))
static void destroy(struct laptop_device_s *self) {
    iio_device_accel_close(&((minibookx_t*)self)->screen);
    iio_device_accel_close(&((minibookx_t*)self)->base);
    free((minibookx_t*)self);
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
        int fd = open(buffer, O_WRONLY);
        if (fd <= 0) {
            make_errorf(error, "Cannot open the %s, error: %s", buffer, strerror(errno));
            return false;
        }
        if (write(fd, "mxc4005 0x15\n", 13) != 13) {
            make_errorf(error, "Cannot write 'mxc4005 0x15' to the %s", buffer);
            return false;
        }
        close(fd);
        sleep(1); // Wait for the device
        if (!iio_device_is_available(1)) {
            make_errorf(error, "Cannot enable the base accelerometer: id = 1, i2c = %d", (int)i2c);
            return false;
        }
    }
    // Accelerometers enabled
    minibookx_t *mdevice = (minibookx_t*)malloc(sizeof(minibookx_t));
    if (!(iio_device_accel_open(0, &mdevice->screen, error) && iio_device_accel_open(1, &mdevice->base, error))) {
        return false;
    }
    mdevice->device.read_screen_accel = &read_screen_accel;
    mdevice->device.read_base_accel = &read_base_accel;
    mdevice->device.destroy = &destroy;
    *device = (laptop_device_t *)mdevice;
    return true;
}

const laptop_device_factory_t device_minibook_x = {
    .is_current_device = &is_current_device,
    .create = &create
};
