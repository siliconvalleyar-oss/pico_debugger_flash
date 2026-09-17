# Blink Firmware for Raspberry Pi Pico 1

Simple firmware that blinks the onboard LED (GPIO 25) every 500ms.

## Hardware Setup

### Debug Probe (Programmer Pico)
- Raspberry Pi Pico 1 running **picoprobe/debugprobe firmware**
- Connected to host PC via USB

### Target Pico (Device to Program)
- Raspberry Pi Pico 1 (RP2040)
- SWD connection from Debug Probe:
  - **Debug Probe GP2 (SWDIO)** → **Target GP2 (SWDIO)**
  - **Debug Probe GP3 (SWCLK)** → **Target GP3 (SWCLK)**
  - **Debug Probe GND** → **Target GND**
  - **Debug Probe 3V3** → **Target 3V3** (optional, if not powered separately)

### Connection Diagram
```
Debug Probe (Programmer)          Target Pico
┌─────────────────────┐          ┌─────────────────────┐
│  GP2  ──────────────┼──────────┤  GP2 (SWDIO)        │
│  GP3  ──────────────┼──────────┤  GP3 (SWCLK)        │
│  GND  ──────────────┼──────────┤  GND                │
│  3V3  ──────────────┼──────────┤  3V3 (optional)     │
└─────────────────────┘          └─────────────────────┘
```

## Building

```bash
cd /mnt/disk/src/rpico/pico_src/blink
./build.sh
```

Outputs:
- `build/blink.uf2` - For drag-and-drop programming (BOOTSEL mode)
- `build/blink.elf` - For SWD programming via Debug Probe

## Programming via Debug Probe (SWD)

### Option 1: Build and program in one step
```bash
./build_and_program.sh
```

### Option 2: Program existing build
```bash
./program.sh
```

### Manual programming
```bash
sudo LD_LIBRARY_PATH=/mnt/disk/src/rpico/hidapi-install/lib \
    /mnt/disk/src/rpico/openocd-src/src/openocd \
    -s /mnt/disk/src/rpico/openocd-src/tcl \
    -f /mnt/disk/src/rpico/debugprobe-openocd.cfg \
    -c "program build/blink.elf verify reset exit"
```

## Programming via BOOTSEL (Drag & Drop)

1. Hold BOOTSEL button on Target Pico
2. Connect Target Pico to PC via USB
3. Release BOOTSEL
4. Copy `build/blink.uf2` to the RPI-RP2 drive

## Files

- `blink.c` - Main firmware (blinks GPIO 25 at 500ms interval)
- `CMakeLists.txt` - CMake build configuration
- `build.sh` - Build script
- `program.sh` - Program via Debug Probe (requires sudo)
- `build_and_program.sh` - Combined build and program
- `README.md` - This file

## Requirements

- Pico SDK at `/mnt/disk/src/rpico/pico-sdk`
- OpenOCD with CMSIS-DAP support at `/mnt/disk/src/rpico/openocd-src/src/openocd`
- hidapi library at `/mnt/disk/src/rpico/hidapi-install/lib`
- Debug Probe firmware loaded on programmer Pico
- udev rules configured for Debug Probe (or run with sudo)

## udev Rules (to avoid sudo)

```bash
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", ATTR{idProduct}=="000c", MODE="0666", GROUP="plugdev"' | sudo tee /etc/udev/rules.d/99-debugprobe.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then reconnect the Debug Probe.