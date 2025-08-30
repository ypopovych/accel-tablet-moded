#define _GNU_SOURCE
#include "device.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/input.h>

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
    make_errorf(error, "Cannot open the accel value: %s", path);
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

bool iio_device_available(uint8_t device_id) {
  struct stat st;
  char path[DEVICE_MAX_PATH] = {0};
  if (snprintf(path, DEVICE_MAX_PATH, IIO_DEVICE_PATH, (unsigned int)device_id) <= 0) {
    return false;
  }
  return stat(path, &st) == 0;
}

bool iio_device_get_i2c_port(uint8_t device_id, uint8_t *port) {
  char path[DEVICE_MAX_PATH] = {0};
  char buffer[DEVICE_MAX_PATH] = {0};
  if (snprintf(path, DEVICE_MAX_PATH, IIO_DEVICE_PATH, (unsigned int)device_id) <= 0) {
    return false;
  }
  ssize_t len = readlink(path, buffer, sizeof(buffer)-2);
  if (len <= 0) {
    return false;
  }
  buffer[len] = '\0';
  char *device_str = strstr(buffer, "/i2c-");
  if (device_str == NULL) {
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

bool iio_read_accel_scale(uint8_t device_id, float *scale, char **error) {
  char path[DEVICE_MAX_PATH] = {0};
  if (snprintf(path, DEVICE_MAX_PATH, IIO_ACCEL_SCALE_PATH, (unsigned int)device_id) <= 0) {
    make_errorf(error, "Can't build iio accel scale path for device: %u", (unsigned int)device_id);
    return false;
  }
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    make_errorf(error, "Cannot open the accel scale: %s", path);
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

bool iio_read_accel_raw_state(uint8_t device_id, accel_state_t* state, char **error) {
  return iio_read_accel_value(device_id, 'x', &state->x, error) &&
         iio_read_accel_value(device_id, 'y', &state->y, error) &&
         iio_read_accel_value(device_id, 'z', &state->z, error);
}

int laptop_device_get_model(char **model) {
  FILE *fp = fopen("/sys/devices/virtual/dmi/id/product_name", "r");
  if (fp == NULL) {
    return -1;
  }
  char buff[DEVICE_MAX_PATH];
  char *result = fgets(buff, DEVICE_MAX_PATH, fp);
  fclose(fp);
  if (result == NULL) {
    return -1;
  }
  size_t len = strlen(result);
  *model = (char *)malloc(len+1);
  strncpy(*model, result, len+1);
  return len-1;
}

int input_get_event_path_by_name(const char *name, char **path) {
  struct dirent **entry;
  int ndevice = scandir("/dev/input", &entry, NULL, versionsort);
  if (ndevice < 0) {
    return -1;
  }
  
  char file_name[DEVICE_MAX_PATH];
  char device_name[DEVICE_MAX_PATH];
  for (int i = 0; i < ndevice; i++) {
    file_name[0] = device_name[0] = '\0';
    // Get the event device name
    sprintf(file_name, "/dev/input/%s", entry[i]->d_name);
    free(entry[i]);
    
    int fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        continue;
    }
    ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name);
    close(fd);

    // Compare the device name
    if (strncmp(device_name, name, DEVICE_MAX_PATH) == 0) {
      // cleanup data
      for (int j = i; j < ndevice; j++) {
        free(entry[j]);
      }
      free(entry);
      // copy output
      size_t len = strlen(file_name)+1;
      *path = (char *)malloc(len);
      strncpy(*path, file_name, len);
      return len-1;
    }
  }
  
  free(entry);
  return -1;
}
