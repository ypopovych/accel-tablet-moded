#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include "device.h"
#include "debug.h"

#define DEVICE_MAX_PATH 256
#define IIO_DEVICE_PATH "/sys/bus/iio/devices/iio:device%u"
#define IIO_ACCEL_SCALE_PATH IIO_DEVICE_PATH"/in_accel_scale"
#define IIO_ACCEL_VALUE_PATH IIO_DEVICE_PATH"/in_accel_%c_raw"

static bool iio_read_accel_value(uint8_t device_id, char axis, double *value, char **error) {
    char path[DEVICE_MAX_PATH] = {0};
    if (snprintf(path, DEVICE_MAX_PATH, IIO_ACCEL_VALUE_PATH, (unsigned int)device_id, axis) <= 0) {
        make_errorf(error, "Can't build iio accel value path for device: %u, axis: %c", (unsigned int)device_id, axis);
        return false;
    }
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        make_errorf(error, "Cannot open the accel value: %s, error: %s", path, strerror(errno));
        return false;
    }
    bool success = fscanf(fp, "%lf", value) > 0;
    fclose(fp);
    if (!success) {
        make_errorf(error, "Cannot read the accel value from: %s", path);
        return false;
    }
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

bool iio_device_accel_read_scale(uint8_t device_id, float *scale, char **error) {
    char path[DEVICE_MAX_PATH] = {0};
    if (snprintf(path, DEVICE_MAX_PATH, IIO_ACCEL_SCALE_PATH, (unsigned int)device_id) <= 0) {
        make_errorf(error, "Can't build iio accel scale path for device: %u", (unsigned int)device_id);
        return false;
    }
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        make_errorf(error, "Cannot open the accel scale: %s, error: %s", path, strerror(errno));
        return false;
    }
    bool success = fscanf(fp, "%f", scale) > 0;
    fclose(fp);
    if (!success) {
        make_errorf(error, "Cannot read the accel scale from: %s", path);
        return false;
    }
    return true;
}

bool iio_device_accel_read_raw_state(uint8_t device_id, accel_state_t* state, char **error) {
    return iio_read_accel_value(device_id, 'x', &state->x, error) &&
           iio_read_accel_value(device_id, 'y', &state->y, error) &&
           iio_read_accel_value(device_id, 'z', &state->z, error);
}

bool laptop_device_get_model(char **model, char **error) {
    FILE *fp = fopen("/sys/devices/virtual/dmi/id/product_name", "r");
    if (fp == NULL) {
        make_errorf(error, "Can't open DMI device: %s", strerror(errno));
        return false;
    }
    char buff[DEVICE_MAX_PATH];
    char *result = fgets(buff, DEVICE_MAX_PATH, fp);
    int fgets_errno = errno;
    fclose(fp);
    if (result == NULL) {
        make_errorf(error, "Can't read DMI device: %s", strerror(fgets_errno));
        return false;
    }
    size_t len = strlen(result)+1;
    *model = (char *)malloc(len);
    strncpy(*model, result, len);
    return true;
}
