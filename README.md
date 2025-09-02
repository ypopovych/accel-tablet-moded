# accel-tablet-moded

A lightweight daemon that enables automatic tablet mode switching on 2-in-1 convertible laptops with dual accelerometers.

## Overview

`accel-tablet-moded` monitors accelerometer data from both the screen and base of convertible laptops to automatically detect when the device is being used in tablet mode. When tablet mode is detected, it sends appropriate input events to the system to trigger UI adaptations and touch-friendly interfaces.

## Features

- **Automatic Detection**: Uses dual accelerometer data to intelligently detect tablet mode transitions
- **Smart Angle Calculation**: Calculates relative angles between screen and base orientations
- **Lid Switch Integration**: Monitors lid switch status to prevent false positives when closed
- **Virtual Input Device**: Creates a standard tablet switch input device recognized by desktop environments
- **Configurable Polling**: Adjustable update frequency for optimal performance vs. battery life
- **Debug Mode**: Detailed logging for troubleshooting and development
- **Service Integration**: Includes dinit service files for automatic startup
- **Low Resource Usage**: Minimal CPU and memory footprint

## Supported Devices

Currently supports:
- **Chuwi MiniBook X** - Full support with automatic base accelerometer activation

The architecture is designed to be extensible for additional devices. See [Adding Device Support](#adding-device-support) for details.

## Requirements

- Linux system with IIO (Industrial I/O) subsystem
- Root privileges (required for hardware access and input device creation)
- Compatible 2-in-1 device with dual accelerometers

## Installation

### Build from Source

```bash
# Clone the repository
git clone https://github.com/ypopovych/accel-tablet-moded.git
cd accel-tablet-moded

# Build the daemon
make

# Install system-wide (requires root)
sudo make install

# Optional: Install dinit service (if using dinit)
sudo make dinit
```

### Manual Installation

```bash
# Copy binary to system path
sudo cp bin/accel-tablet-moded /usr/bin/
sudo chmod 755 /usr/bin/accel-tablet-moded

# Optional: Copy service files
sudo cp services/dinit.service /usr/lib/dinit.d/accel-tablet-moded
sudo cp services/dinit.conf /etc/default/accel-tablet-moded
```

## Usage

### Command Line Options

```bash
accel-tablet-moded [OPTIONS]

Options:
  -f <time>      Polling frequency in seconds (default: 1.0)
  -d, --debug    Enable debug mode with detailed logging
  -h, --help     Show help message
  -v, --version  Display version information
```

### Examples

```bash
# Run with default settings
sudo accel-tablet-moded

# Run with faster polling (0.5 seconds)
sudo accel-tablet-moded -f 0.5

# Run in debug mode
sudo accel-tablet-moded --debug

# Run in background with custom frequency
sudo accel-tablet-moded -f 2.0 &
```

### Service Configuration

Edit `/etc/default/accel-tablet-moded` to configure service options:

```bash
# Polling frequency in seconds
UPDATE_FREQUENCY=1.0

# Enable debug mode (uncomment to activate)
#DEBUG="-d"
```

## How It Works

1. **Device Detection**: Automatically identifies supported hardware by checking system model information
2. **Accelerometer Setup**: Initializes and configures screen and base accelerometers via IIO subsystem
3. **Continuous Monitoring**: Polls accelerometer data at configurable intervals
4. **Angle Calculation**: Computes relative orientation between screen and base using XZ-plane angles
5. **Mode Detection**: Applies threshold logic to determine tablet vs. laptop mode
6. **Input Events**: Sends tablet switch events through virtual input device
7. **Lid Switch Integration**: Monitors lid state to prevent tablet mode when closed

### Detection Algorithm

The daemon uses the following logic:
- Calculates angles from accelerometer X and Z values
- Determines relative angle between screen and base orientations
- Triggers tablet mode when angle indicates folded-back configuration
- Includes hysteresis to prevent rapid mode switching
- Respects lid switch state for reliable operation

## Configuration

### Polling Frequency

The `-f` parameter controls how often accelerometer data is read:
- **Higher frequency** (e.g., 0.1s): More responsive, higher CPU usage
- **Lower frequency** (e.g., 2.0s): Lower CPU usage, less responsive
- **Recommended**: 0.5-1.0 seconds for optimal balance

### Debug Mode

Enable debug mode to see real-time accelerometer values and mode decisions:
```bash
sudo accel-tablet-moded -d
```

Output includes:
- Raw accelerometer readings for screen and base
- Calculated angles and relative orientation
- Current tablet mode state
- Lid switch status

## Adding Device Support

To add support for new devices:

1. Create a new device file in `devices/` directory
2. Implement the `laptop_device_factory_t` interface:
   - `is_current_device()` - Device detection logic
   - `create()` - Device initialization
3. Implement `laptop_device_t` methods:
   - `read_screen_accel()` - Read screen accelerometer
   - `read_base_accel()` - Read base accelerometer  
   - `destroy()` - Cleanup resources
4. Add device factory to `G_all_devices` array in `daemon.c`

See `devices/minibook_x.c` for a complete implementation example.

## Troubleshooting

### Common Issues

**Daemon fails to start:**
- Ensure you're running as root
- Check that IIO devices are available: `ls /sys/bus/iio/devices/`
- Verify accelerometers are detected: `cat /sys/bus/iio/devices/iio:device*/name`

**No tablet mode switching:**
- Run with debug mode to see sensor readings
- Check if lid switch is properly detected
- Verify desktop environment supports tablet mode switching

**High CPU usage:**
- Increase polling frequency with `-f` option
- Check for excessive debug output in logs

### Debug Information

Run in debug mode to diagnose issues:
```bash
sudo accel-tablet-moded -d
```

Check system logs for error messages:
```bash
journalctl -u accel-tablet-moded
```

## Contributing

Contributions are welcome! Areas where help is needed:
- Support for additional 2-in-1 devices
- Improved detection algorithms
- Power management optimizations
- Desktop environment integration testing

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.

## Changelog

### 0.1.0
* All basic logic.
* Chuwi MiniBook X support.
