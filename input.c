#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define _GNU_SOURCE
#include <dirent.h>

#include "input.h"
#include "debug.h"

// Emit the event
static bool emit(int fd, int type, int code, int value) {
    struct input_event event = {.type = type, .code = code, .value = value};
    // Set the timestamp (dummy)
    event.time.tv_sec = 0;
    event.time.tv_usec = 0;
    return write(fd, &event, sizeof(event)) == sizeof(event);
}

bool input_device_tablet_switch_create(int *fd, char **error) {
    *fd = -1;
    int dev = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (dev < 0) {
        make_errorf(error, "Can't open /dev/uinput device: %s", strerror(errno));
        return false;
    }

    // Enable the synchronization events
    if (ioctl(dev, UI_SET_EVBIT, EV_SYN) < 0) {
        close(dev);
        make_errorf(error, "Can't enable SYN events: %s", strerror(errno));
        return false;
    }

    // Enable the switch events
    if (ioctl(dev, UI_SET_EVBIT, EV_SW) < 0) {
        close(dev);
        make_errorf(error, "Can't enable SW events: %s", strerror(errno));
        return false;
    }

    // Enable the tabletmode switch
    if (ioctl(dev, UI_SET_SWBIT, SW_TABLET_MODE) < 0) {
        close(dev);
        make_errorf(error, "Can't enable SW_TABLET_MODE events: %s", strerror(errno));
        return false;
    }

    // Setup the device
    struct uinput_setup uisetup = {0};
    strcpy(uisetup.name, "Accelerometer Tablet Mode Virtual Switch");
    uisetup.id.bustype = BUS_USB;
    uisetup.id.vendor = 0x1234;
    uisetup.id.product = 0x5678;
    uisetup.id.version = 1;

    if (ioctl(dev, UI_DEV_SETUP, &uisetup) < 0) {
        close(dev);
        make_errorf(error, "Failed to setup the virtual device: %s", strerror(errno));
        return false;
    }

    if (ioctl(dev, UI_DEV_CREATE) < 0) {
        close(dev);
        make_errorf(error, "Failed to create the virtual device: %s", strerror(errno));
        return false;
    }
    
    *fd = dev;
    return true;
}

bool input_device_tablet_switch_set_mode(int fd, bool value, char **error) {
    if (!emit(fd, EV_SW, SW_TABLET_MODE, (int)value)) {
        make_errorf(error, "Can't write switch value %s", strerror(errno));
        return false;
    }
    if (!emit(fd, EV_SYN, SYN_REPORT, 0)) {
        make_errorf(error, "Can't write switch syn event: %s", strerror(errno));
        return false;
    }
    return true;
}

void input_device_tablet_switch_destroy(int *fd) {
    if (*fd < 0) return;
    ioctl(*fd, UI_DEV_DESTROY);
    close(*fd);
    *fd = -1;
}

bool input_device_open_named(const char* device_name, int *fd, char **error) {
    char *path = NULL;
    if (!input_device_find_path(device_name, &path, error)) {
        return false;
    }
    bool res = input_device_open(path, fd, error);
    free(path);
    return res;
}

bool input_device_open(const char* path, int *fd, char **error) {
    *fd = open(path, O_RDONLY);
    if (*fd < 0) {
        make_errorf(error, "Cannot open the lid switch: %s, error: %s", path, strerror(errno));
        return false;
    }
    return true;
}

bool input_device_find_path(const char *device_name, char **path, char **error) {
    struct dirent **entry;
    int ndevice = scandir("/dev/input", &entry, NULL, versionsort);
    if (ndevice < 0) {
        make_error(error, "Can't read /dev/input directory contents"); 
        return false;
    }
  
    char entry_file_name[256];
    char entry_device_name[256];
    for (int i = 0; i < ndevice; i++) {
        entry_file_name[0] = entry_device_name[0] = '\0';
        // Get the event device name
        snprintf(entry_file_name, sizeof(entry_file_name), "/dev/input/%s", entry[i]->d_name);
        free(entry[i]);
    
        int fd = open(entry_file_name, O_RDONLY);
        if (fd == -1) {
            continue;
        }
        ioctl(fd, EVIOCGNAME(sizeof(entry_device_name)), entry_device_name);
        close(fd);

        // Compare the device name
        if (strncmp(entry_device_name, device_name, sizeof(entry_device_name)) == 0) {
            // cleanup data
            for (int j = i; j < ndevice; j++) {
                free(entry[j]);
            }
            free(entry);
            // copy output
            size_t len = strlen(entry_file_name)+1;
            *path = (char *)malloc(len);
            strncpy(*path, entry_file_name, len);
            return true;
        }
    }
    free(entry);
    make_errorf(error, "Can't find device '%s'", device_name); 
    return false;
}

bool input_device_lid_switch_read(int fd, struct timespec timeout, bool *is_lid_closed, char **error) {
    struct input_event ev;
  
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
        .revents = 0
    };
  
    int rpoll = 0;
    bool result = true;
    do {
        rpoll = ppoll(&pfd, 1, &timeout, NULL);
        if (rpoll < 0) {
            result = false;
            if (errno != EINTR) {
                make_errorf(error, "Lid poll error: %s", strerror(errno));
            }
        } else if (rpoll > 0) {
            int bytes = read(fd, &ev, sizeof(ev));
            if (bytes <= 0) {
                result = false;
                if (bytes < 0) {
                    make_errorf(error, "Lid read error: %s", strerror(errno));
                } else {
                    make_error(error, "Lid read error: got 0 bytes");
                }
            } else {
                if (ev.type == EV_SW && ev.code == SW_LID) {
                    if (ev.value == 1) {
                        debug("Lid closed\n");
                        *is_lid_closed = true;
                    } else {
                        debug("Lid opened\n");
                        *is_lid_closed = false;
                    }
                }
            }
        }
  } while (rpoll > 0 && result);
  
  return result;
}

void input_device_close(int *fd) {
    if (*fd < 0) return;
    close(*fd);
    *fd = -1;
}

