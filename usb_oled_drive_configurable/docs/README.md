# Pico USB Drive Configurable

A **professional USB pendrive** built on the Raspberry Pi Pico RP2040 whose
storage lives in the **onboard QSPI flash (2 MB..16 MB, auto-detected at boot
and formatted as FAT/exFAT)**.

Unlike a volatile RAM ramdisk, this drive **persists its contents across
power cycles** because it writes to the Pico's SPI flash. Everything is
configurable both at *compile time* (`src/config.h`) and at *runtime*
(`config.txt` on the drive itself).

---

## Features

- **Persistent storage** in the Pico's onboard QSPI flash — 1.5 MB disk on a
  2 MB Pico, **15 MB disk on a 16 MB board** (JEDEC-detected at boot).
- **USB Mass Storage (MSC)**: the Pico enumerates as a normal pendrive.
- **Runtime configuration** via `config.txt` (no rebuild, no re-flash):
  - `VOLUME_LABEL`
  - `READ_ONLY`
  - `ENABLE_OLED`
  - `LED_ON_CONNECT`
  - `AUTO_MOUNT_DELAY_MS`
- **Hot-plug configuration watcher**: edits to `config.txt` are detected
  (polling + content hash) and applied **every 2 seconds without a reboot**.
- **Auto-created config**: on first boot, `config.txt` is written with the
  default values from `config.h`.
- **SSD1306 OLED (128x64)** status screen: volume label, read-only lock,
  connected state, total/free capacity and config-error hint.
- **Status LED** with distinct blink patterns.
- **Self-formatting**: if the flash region is empty (0xFF/0x00 fill, i.e. brand
  new), it is formatted once as **exFAT** on first boot (FAT32 is impossible
  under ~32 MB and Android does not auto-mount FAT16). A region that already holds
  a filesystem it can read (FAT **or exFAT**, e.g. formatted by Android) is
  mounted as-is and **never wiped** — ending the "ask to format every time you
  plug it in" loop. If it contains data FatFS cannot recognise, it is left
  untouched for the host to re-format explicitly.
- **Volume label** set from config (`VOLUME_LABEL`, default "MiPendrive").
- **Robust against power loss**: FAT buffers are flushed on sync / close.

---

## Project layout

```
pico_usb_drive_configurable/
├── CMakeLists.txt              # top-level build (pulls SDK + FatFS + app)
├── pico_sdk_import.cmake       # SDK locator
├── src/
│   ├── config.h                # ALL compile-time configuration
│   ├── main.cpp                # orchestrator: boot, mount, USB, watcher
│   ├── config_manager.cpp      # config.txt read/write + hot-plug watcher
│   ├── fatfs_interface.cpp     # mount / format / sync the FAT volume
│   ├── usb_storage.cpp         # TinyUSB MSC callbacks -> flash sectors
│   ├── oled_display.cpp        # SSD1306 driver + status rendering
│   ├── gpio_control.cpp        # status LED blink patterns
│   └── tusb_config.h           # TinyUSB (MSC only)
├── include/
│   ├── pendrive.h              # shared config/state + module headers
│   ├── config_manager.h
│   ├── fatfs_interface.h
│   ├── usb_storage.h
│   ├── oled_display.h
│   └── gpio_control.h
├── lib/fatfs/                  # vendored Elm-chan FatFS
│   └── source/
│       ├── ff.c ff.h           # FatFS core
│       ├── ffunicode.c         # Unicode support
│       ├── ffsystem.c          # OS layer (none -> no-ops)
│       ├── diskio.c diskio.h   # flash-backed disk I/O (overridden)
│       └── ffconf.h            # FatFS compile options
└── docs/                       # this documentation set
```

---

## Quick start

1. **Compile-time tuning** (optional): edit `src/config.h`.
2. Build and flash the `.uf2` to the Pico (see `docs/BUILD_INSTRUCTIONS.md`).
3. Plug the Pico into a USB port. It appears as a FAT drive.
4. Open `config.txt` on the drive, edit values, save. Within ~2 s the device
   re-applies the new settings (label, read-only flag, OLED, LED, mount delay)
   — no reboot needed.

---

## Documentation index

| Document | Purpose |
|----------|---------|
| [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) | Build the firmware and flash the Pico |
| [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) | Every config option, runtime `config.txt` + compile-time `config.h` |
| [HARDWARE_SETUP.md](HARDWARE_SETUP.md) | Wiring: OLED, LED, flash notes |
| [API_REFERENCE.md](API_REFERENCE.md) | Internal module API used by `src/` |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Common issues and fixes |
| [LATEST.md](LATEST.md) | Latest changes: write-behind cache, root-dir corruption incident & recovery |

---

## License

MIT — see `lib/fatfs/source/License.txt` for the vendored FatFS license.
