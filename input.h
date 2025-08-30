#pragma once

#include <stdbool.h>
#include <time.h>

bool input_device_tablet_switch_create(int *fd, char **error);
bool input_device_tablet_switch_set_mode(int fd, bool value, char **error);
void input_device_tablet_switch_destroy(int *fd);

bool input_device_open_named(const char* device_name, int *fd, char **error);
bool input_device_open(const char* path, int *fd, char **error);
bool input_device_find_path(const char *device_name, char **path, char **error);
bool input_device_lid_switch_read(int fd, struct timespec timeout, bool *is_lid_closed, char **error);
void input_device_close(int *fd);

