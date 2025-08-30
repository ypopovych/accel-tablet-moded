#include <stdio.h>
#include <stdarg.h> 
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include "device.h"
#include "devices/minibook_x.h"
#include "debug.h"

#define VERSION "0.1.0"

#define CLOSE_OUTPUT(out) ioctl(out, UI_DEV_DESTROY); close(out)

static const laptop_device_factory_t* G_all_devices[] = {
  &device_minibook_x
};

static volatile bool G_is_running = false;

static int exit_with_error(char* error) {
  fprintf(stderr, "%s\n", error);
  free(error);
  return EXIT_FAILURE;
}

// Print the help message
static void print_help() {
    printf("Usage: accel-tablet-moded [-d] [-h] [--version]\n");
    printf("Options:\n");
    printf("  -d: Enable debug mode\n");
    printf("  -h: Print this help message\n");
    printf("  --version: Print the version\n");
}

// Parse the command line arguments
static int parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            set_debug_mode_enabled(true);
            return -1;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("%s\n", VERSION);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_help();
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help();
            return EXIT_FAILURE;
        }
    }
    return -1;
}

// Signal handler
__attribute__((noinline))
static void sigint_handler(int signum) {
    (void)(signum);
    G_is_running = false;
}

// Emit the event
static void emit(int fd, int type, int code, int value) {
    struct input_event event = {.type = type, .code = code, .value = value};
    // Set the timestamp (dummy)
    event.time.tv_sec = 0;
    event.time.tv_usec = 0;
    write(fd, &event, sizeof(event));
}

static void set_tablet_mode(int device, bool value) {
    emit(device, EV_SW, SW_TABLET_MODE, (int)value);
    emit(device, EV_SYN, SYN_REPORT, 0);
}

static int new_uinput_switch_device(char **error) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd == -1) {
      make_error(error, "Can't open /dev/uinput device");
      return -1;
    }

    // Enable the synchronization events
    if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0) {
      close(fd);
      make_error(error, "Can't enable SYN events");
      return -2;
    }

    // Enable the switch events
    if (ioctl(fd, UI_SET_EVBIT, EV_SW) < 0) {
      close(fd);
      make_error(error, "Can't enable SW events");
      return -3;
    }

    // Enable the tabletmode switch
    if (ioctl(fd, UI_SET_SWBIT, SW_TABLET_MODE) < 0) {
      close(fd);
      make_error(error, "Can't enable SW_TABLET_MODE events");
      return -4;
    }

    // Setup the device
    struct uinput_setup uisetup = {0};
    strcpy(uisetup.name, "Accelerometer Tablet Mode Virtual Switch");
    uisetup.id.bustype = BUS_USB;
    uisetup.id.vendor = 0x1234;
    uisetup.id.product = 0x5678;
    uisetup.id.version = 1;

    if (ioctl(fd, UI_DEV_SETUP, &uisetup) < 0) {
        close(fd);
        make_error(error, "Failed to setup the virtual device");
        return -5;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        close(fd);
        make_error(error, "ailed to create the virtual device");
        return -6;
    }
    
    return fd;
}

static bool read_lid_switch(int fd, bool *is_lid_closed, char **error) {
  struct input_event ev;
  
  struct pollfd pfd = {
    .fd = fd,
    .events = POLLIN,
    .revents = 0
  };
  struct timespec timeout = {
    .tv_sec = 1,
    .tv_nsec = 0
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

static bool open_lid_switch(int* fd, char **error) {
  char *path = NULL;
  int len = input_get_event_path_by_name("Lid Switch", &path);
  if (len <= 0) {
    make_error(error, "Cannot find the lid switch device");
    return false;
  }
  *fd = open(path, O_RDONLY);
  if (*fd < 0) {
    make_errorf(error, "Cannot open the lid switch: %s, error: %s", path, strerror(errno));
    free(path);
    return false;
  }
  free(path);
  return true;
}


// Main
int main(int argc, char *argv[]) {
    // Parse the command line arguments
    int arg_result = parse_args(argc, argv);
    if (arg_result >= 0) {
      return arg_result;
    }
    
    char *error = NULL;
    int output = new_uinput_switch_device(&error);
    if (output < 0) {
        return exit_with_error(error);
    }

    // Register the signal handler
    signal(SIGINT, sigint_handler);

    // Start the lid switch thread
    int lid_switch_fd = -1;
    if (!open_lid_switch(&lid_switch_fd, &error)) {
      CLOSE_OUTPUT(output);
      return exit_with_error(error);
    }
    
    // Get laptop model
    char* laptop_model = NULL;
    if (laptop_device_get_model(&laptop_model) <= 0) {
      CLOSE_OUTPUT(output);
      close(lid_switch_fd);
      make_error(&error, "Can't get laptop model");
      return exit_with_error(error);
    }
    
    debug("Laptop model: %s\n", laptop_model);
    
    int devices_len = sizeof(G_all_devices) / sizeof(laptop_device_factory_t*);
    const laptop_device_factory_t* device_factory = NULL;
    for (int i = 0; i < devices_len; i++) {
      if (G_all_devices[i]->is_current_device(laptop_model)) {
        device_factory = G_all_devices[i];
        break;
      }
    }
    if (device_factory == NULL) {
      CLOSE_OUTPUT(output);
      close(lid_switch_fd);
      make_errorf(&error, "Unsupported laptop model: %s", laptop_model);
      free(laptop_model);
      return exit_with_error(error);
    }
    free(laptop_model);
    
    laptop_device_t *device = NULL;
    if (!device_factory->create(&device, &error)) {
      CLOSE_OUTPUT(output);
      close(lid_switch_fd);
      return exit_with_error(error);
    }
    
    bool is_tablet_mode_enabled = false;
    bool is_lid_closed = false;
    accel_state_t screen_state;
    accel_state_t base_state;
    
    G_is_running = true;
    error = NULL;
    
    while (G_is_running) {
      if (!read_lid_switch(lid_switch_fd, &is_lid_closed, &error)) {
        break;
      }
      if (is_tablet_mode_enabled && is_lid_closed) {
        set_tablet_mode(output, false);
        is_tablet_mode_enabled = false;
      }
      if (!is_lid_closed) {
        if (!device->read_screen_accel(device, &screen_state, &error)) {
          break;
        }
        if (!device->read_base_accel(device, &base_state, &error)) {
          break;
        }
        
        debug("Screen: x:%lf y:%lf z:%lf\n", screen_state.x, screen_state.y, screen_state.z);
        debug("Base  : x:%lf y:%lf z:%lf\n", base_state.x, base_state.y, base_state.z);

        // Get the angle from x, z
        double angle_screen = accel_state_get_xz_angle(&screen_state);
        double angle_base = accel_state_get_xz_angle(&base_state);
        double angle = angle_base - angle_screen;

        if (angle < 0 && angle_base < 0 && angle_screen > 0) {
            angle += 360.0;
        }

        if (screen_state.x > 3.0 || screen_state.x < -3.0 ||
            screen_state.z > 3.0 || screen_state.z < -3.0)
        {
            if (360 - angle < 60 && angle > 0 && !is_tablet_mode_enabled) {
              set_tablet_mode(output, true);
              is_tablet_mode_enabled = true;
            } else if (angle < 10 && angle > -60 && !is_tablet_mode_enabled) {
              set_tablet_mode(output, true);
              is_tablet_mode_enabled = true;
            } else if (angle > 10 && angle < 180 && is_tablet_mode_enabled) {
              set_tablet_mode(output, false);
              is_tablet_mode_enabled = false;
            }
        }
        
        debug("angle_screen: %lf\n", angle_screen);
        debug("angle_base: %lf\n", angle_base);
        debug("diff: %lf\n", angle);
        debug("tablet_mode: %s\n", is_tablet_mode_enabled ? "true" : "false");
      }
    }
    
    G_is_running = false;
    
    CLOSE_OUTPUT(output);
    close(lid_switch_fd);
    device->destroy(device);
    
    if (error != NULL) {
      return exit_with_error(error);
    }
    
    return EXIT_SUCCESS;
}
