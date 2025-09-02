#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include "device.h"
#include "debug.h"

#define DEVICE_MAX_PATH 256
#define IIO_DEVICE_PATH "/sys/bus/iio/devices/iio:device%u"
#define IIO_ACCEL_SCALE_PATH IIO_DEVICE_PATH"/in_accel_scale"
#define IIO_ACCEL_VALUE_PATH IIO_DEVICE_PATH"/in_accel_%c_raw"

static bool iio_device_accel_open_axis(uint8_t device_id, char axis, int *fd, char **error) {
    char path[DEVICE_MAX_PATH] = {0};
    if (snprintf(path, DEVICE_MAX_PATH, IIO_ACCEL_VALUE_PATH, (unsigned int)device_id, axis) <= 0) {
        make_errorf(error, "Can't build iio accel value path for device: %u, axis: %c", (unsigned int)device_id, axis);
        return false;
    }
    *fd = open(path, O_RDONLY);
    if (*fd <= 0) {
        make_errorf(error, "Cannot open the accel value: %s, error: %s", path, strerror(errno));
        return false;
    }
    return true;
}

static inline bool iio_read_double_value(int fd, char buffer[20], double *value) {
    // read string value of accelerometer
    size_t len = read(fd, buffer, 19);
    // len should be greater than 0
    if (len <= 0) {
        return false;
    }
    // Seek to the start of device info
    lseek(fd, 0, SEEK_SET);
    // terminate string (if not terminated)
    buffer[len] = '\0';
    // convert value to double
    *value = atof(buffer);
    return true;
}

bool iio_device_is_available(uint8_t device_id) {
    struct stat st;
    char path[DEVICE_MAX_PATH] = {0};
    if (snprintf(path, DEVICE_MAX_PATH, IIO_DEVICE_PATH, (unsigned int)device_id) <= 0) {
        return false;
    }
    return stat(path, &st) == 0;
}

bool iio_device_get_i2c_port(uint8_t device_id, uint8_t *port, char** error) {
    char path[DEVICE_MAX_PATH] = {0};
    char buffer[DEVICE_MAX_PATH] = {0};
    if (snprintf(path, DEVICE_MAX_PATH, IIO_DEVICE_PATH, (unsigned int)device_id) <= 0) {
        make_error(error, "Can't sprint device path");
        return false;
    }
    ssize_t len = readlink(path, buffer, sizeof(buffer)-2);
    if (len <= 0) {
        make_errorf(error, "Can't read link for %s, error: %s", path, strerror(errno));
        return false;
    }
    buffer[len] = '\0';
    char *device_str = strstr(buffer, "/i2c-");
    if (device_str == NULL) {
        make_errorf(error, "Can't find '/i2c-' in %s", buffer);
        return false;
    }
    int i = 0;
    device_str = device_str + 5;
    while (isdigit(device_str[i]) && i < 3) {
        i++;
    }
    device_str[i] = '\0';
    *port = (uint8_t)atoi(device_str);
    return i > 0;
}

bool iio_device_accel_open(uint8_t device_id, accel_device_t *device, char** error) {
    if (!iio_device_accel_read_scale(device_id, &device->scale, error)) {
        return false;
    }
    if (!iio_device_accel_open_axis(device_id, 'x', &device->fd_x, error)) {
        return false;
    }
    if (!iio_device_accel_open_axis(device_id, 'y', &device->fd_y, error)) {
        close(device->fd_x);
        return false;
    }
    if (!iio_device_accel_open_axis(device_id, 'z', &device->fd_z, error)) {
        close(device->fd_x);
        close(device->fd_y);
        return false;
    }
    return true;
}

void iio_device_accel_close(accel_device_t *device) {
    close(device->fd_x);
    close(device->fd_y);
    close(device->fd_z);
    device->fd_x = device->fd_y = device->fd_z = -1;
}

bool iio_device_accel_read_scale(uint8_t device_id, double *scale, char **error) {
    char path[DEVICE_MAX_PATH] = {0};
    char value_buffer[20] = {0};
    if (snprintf(path, DEVICE_MAX_PATH, IIO_ACCEL_SCALE_PATH, (unsigned int)device_id) <= 0) {
        make_errorf(error, "Can't build iio accel scale path for device: %u", (unsigned int)device_id);
        return false;
    }
    int fd = open(path, O_RDONLY);
    if (fd <= 0) {
        make_errorf(error, "Cannot open the accel scale: %s, error: %s", path, strerror(errno));
        return false;
    }
    bool success = iio_read_double_value(fd, value_buffer, scale);
    close(fd);
    if (!success) {
        make_errorf(error, "Cannot read the accel scale from: %s", path);
        return false;
    }
    return true;
}

bool iio_device_accel_read_state(const accel_device_t *device, accel_state_t *state, char **error) {
    char value_buffer[20] = {0};
    if (!iio_read_double_value(device->fd_x, value_buffer, &state->x)) {
        make_error(error, "Cannot read the accel value for axis x");
        return false;
    }
    if (!iio_read_double_value(device->fd_y, value_buffer, &state->y)) {
        make_error(error, "Cannot read the accel value for axis y");
        return false;
    }
    if (!iio_read_double_value(device->fd_z, value_buffer, &state->z)) {
        make_error(error, "Cannot read the accel value for axis z");
        return false;
    }
    accel_state_apply_scale(state, device->scale);
    return true;
}

bool laptop_device_get_model(char **model, char **error) {
    int fd = open("/sys/devices/virtual/dmi/id/product_name", O_RDONLY);
    if (fd <= 0) {
        make_errorf(error, "Can't open DMI device: %s", strerror(errno));
        return false;
    }
    char buffer[DEVICE_MAX_PATH];
    size_t len = read(fd, buffer, DEVICE_MAX_PATH-1);
    int read_errno = errno;
    close(fd);
    if (len <= 0) {
        make_errorf(error, "Can't read DMI device: %s", strerror(read_errno));
        return false;
    }
    buffer[len] = '\0';
    *model = (char *)malloc(len+1);
    strncpy(*model, buffer, len+1);
    return true;
}
