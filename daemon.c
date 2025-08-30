#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#include "input.h"
#include "device.h"
#include "devices/minibook_x.h"
#include "debug.h"

#define VERSION "0.1.0"

static const laptop_device_factory_t* G_all_devices[] = {
  &device_minibook_x
};

static volatile bool G_is_running = false;

inline static int exit_with_error(char* error) {
  fprintf(stderr, "%s\n", error);
  free(error);
  return EXIT_FAILURE;
}

// Print the help message
inline static void print_help() {
    printf("Usage: accel-tablet-moded [-d] [-h] [--version]\n");
    printf("Options:\n");
    printf("  -d: Enable debug mode\n");
    printf("  -h: Print this help message\n");
    printf("  --version: Print the version\n");
}

// Parse the command line arguments
inline static int parse_args(int argc, char *argv[]) {
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

inline static laptop_device_t* create_laptop_device(const laptop_device_factory_t* const* devices, size_t devices_len, char **error) {
    // Get laptop model
    char* laptop_model = NULL;
    if (!laptop_device_get_model(&laptop_model, error)) {
        return NULL;
    }
    
    debug("Laptop model: %s\n", laptop_model);
    
    int model_len = strlen(laptop_model);
    const laptop_device_factory_t* device_factory = NULL;
    for (size_t i = 0; i < devices_len; i++) {
        if (devices[i]->is_current_device(laptop_model, model_len)) {
            device_factory = devices[i];
            break;
        }
    }
    if (device_factory == NULL) {
        make_errorf(error, "Unsupported laptop model: %s", laptop_model);
        free(laptop_model);
        return NULL;
    }
    free(laptop_model);
    
    laptop_device_t *device = NULL;
    if (!device_factory->create(&device, error)) {
        return NULL;
    }
    return device;
}

// Signal handler
__attribute__((noinline))
static void sigint_handler(int signum) {
    (void)(signum);
    G_is_running = false;
}

// Main
int main(int argc, char *argv[]) {
    // Parse the command line arguments
    int arg_result = parse_args(argc, argv);
    if (arg_result >= 0) {
        return arg_result;
    }
    
    char *error = NULL;
    
    // Create virtual switch device
    int switch_device = -1;
    if (!input_device_tablet_switch_create(&switch_device, &error)) {
        return exit_with_error(error);
    }

    // Register the signal handler
    signal(SIGINT, sigint_handler);

    // Open lid switch device for polling
    int lid_switch_device = -1;
    if (!input_device_open_named("Lid Switch", &lid_switch_device, &error)) {
        input_device_tablet_switch_destroy(&switch_device);
        return exit_with_error(error);
    }
    
    size_t devices_len = sizeof(G_all_devices) / sizeof(laptop_device_factory_t*);
    laptop_device_t *device = create_laptop_device(G_all_devices, devices_len, &error);
    if (device == NULL) {
        input_device_tablet_switch_destroy(&switch_device);
        input_device_close(&lid_switch_device);
        return exit_with_error(error);
    }
    
    bool is_tablet_mode_enabled = false;
    bool is_lid_closed = false;
    accel_state_t screen_state;
    accel_state_t base_state;
    
    G_is_running = true;
    error = NULL;
    
    struct timespec timeout = {
        .tv_sec = 1,
        .tv_nsec = 0
    };
    
    while (G_is_running) {
        if (!input_device_lid_switch_read(lid_switch_device, timeout, &is_lid_closed, &error)) {
            break;
        }
        if (is_tablet_mode_enabled && is_lid_closed) {
            if (!input_device_tablet_switch_set_mode(switch_device, false, &error)) {
                break;
            }
            is_tablet_mode_enabled = false;
        }
        // Lid is closed, do nothing
        if (is_lid_closed) continue;
        
        // Lid is open. Calculate angle
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
                if (!input_device_tablet_switch_set_mode(switch_device, true, &error)) {
                    break;
                }
                is_tablet_mode_enabled = true;
            } else if (angle < 10 && angle > -60 && !is_tablet_mode_enabled) {
                if (!input_device_tablet_switch_set_mode(switch_device, true, &error)) {
                    break;
                }
                is_tablet_mode_enabled = true;
            } else if (angle > 10 && angle < 180 && is_tablet_mode_enabled) {
                if (!input_device_tablet_switch_set_mode(switch_device, false, &error)) {
                    break;
                }
                is_tablet_mode_enabled = false;
            }
        }
        
        debug("angle_screen: %lf\n", angle_screen);
        debug("angle_base: %lf\n", angle_base);
        debug("diff: %lf\n", angle);
        debug("tablet_mode: %s\n\n", is_tablet_mode_enabled ? "true" : "false");
    }
    
    G_is_running = false;
    
    input_device_tablet_switch_destroy(&switch_device);
    input_device_close(&lid_switch_device);
    device->destroy(device);
    
    if (error != NULL) {
      return exit_with_error(error);
    }
    
    return EXIT_SUCCESS;
}
