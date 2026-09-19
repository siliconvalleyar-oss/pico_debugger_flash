# Build Instructions

## Prerequisites

- Raspberry Pi Pico SDK checked out, e.g. at `../../pico-sdk` relative to the
  project root (or set `PICO_SDK_PATH`).
- `arm-none-eabi-gcc` toolchain 13.x or newer.
- `cmake` >= 3.13 and `make`.
- Python 3 (the SDK uses it for some tooling).

The build downloads/locates `picotool` automatically (or you can install it).

## Configure

```bash
cd pico_usb_drive_configurable
mkdir -p build
cd build
cmake ..
```

If the SDK is not in the default location, point at it:

```bash
# either set it at configure time:
cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
# or via environment:
export PICO_SDK_PATH=/path/to/pico-sdk
```

## Build

```bash
make -j"$(nproc)"
```

The output firmware is generated under:

```
build/src/pico_usb_drive_configurable.uf2
```

Other artifacts (`build/src/pico_usb_drive_configurable.bin` / `.elf` /
`.map` / `.uf2`) are also produced by `pico_add_extra_outputs()`.

## Configure the build (optional)

Edit `src/config.h` before building to change the flash layout, GPIO wiring or
USB identity (see `docs/CONFIGURATION_GUIDE.md`).

## Flash the Pico

1. Hold the **BOOTSEL** button on the Pico and plug in the USB — it mounts as
   a 134 MB drive named `RPI-RP2`.
2. Copy `build/src/pico_usb_drive_configurable.uf2` onto it.
3. The Pico reboots automatically and enumerates as the pendrive.

## Verify

```bash
# After plugging the Pico into Linux:
lsblk                      # look for a new FAT drive
sudo dmesg | tail          # should show a USB mass storage that mounts rw
```

The drive should mount **read/write** (see `docs/TROUBLESHOOTING.md` if it
comes up read-only — that is the classic FAT corruption symptom this project
avoids).
