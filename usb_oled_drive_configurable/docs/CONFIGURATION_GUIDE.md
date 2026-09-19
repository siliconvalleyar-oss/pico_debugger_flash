# Configuration Guide

The pendrive has two configuration layers:

1. **Compile-time** (`src/config.h`) — set once before building.
2. **Runtime** (`config.txt`) — edited on the drive itself; applied live.

---

## 1. Runtime configuration (`config.txt`)

On first boot the firmware creates `config.txt` at the root of the drive with
the default values. You can edit it with any text editor (Windows Notepad works
because the file uses CRLF line endings).

### Syntax

```
# This is a comment (starts with '#')
# Whitespace around keys and values is ignored. Keys are case-insensitive.

VOLUME_LABEL=MiPendrive
READ_ONLY=0
ENABLE_OLED=1
LED_ON_CONNECT=1
AUTO_MOUNT_DELAY_MS=500
```

Rules:
- Comment lines start with `#`.
- One `KEY=VALUE` pair per line.
- Keys are matched case-insensitively.
- Blank lines and surrounding whitespace are ignored.
- Unknown keys or invalid values make `config_valid` false and the OLED shows
  `config.txt: error` (valid lines still apply).

### Recognised keys

| Key                  | Type    | Valid values        | Default        | Effect |
|----------------------|---------|---------------------|----------------|--------|
| `VOLUME_LABEL`       | string  | 1..11 chars, no spaces | `MiPendrive` | FAT volume label shown in the OS |
| `READ_ONLY`          | 0/1     | `0` or `1`          | `0`            | `1` = medium becomes read-only |
| `ENABLE_OLED`        | 0/1     | `0` or `1`          | `1`            | `1` = OLED on, `0` = OLED in sleep |
| `LED_ON_CONNECT`     | 0/1     | `0` or `1`          | `1`            | `1` = solid LED when connected |
| `AUTO_MOUNT_DELAY_MS`| integer | `0`..`60000`        | `500`          | Boot delay before USB enumerate |

### Hot-plug (instant apply)

The **Config Watcher** re-reads `config.txt` every 2 s (see
`CONFIG_POLL_INTERVAL_MS`). When the file's content hash changes, the new
settings are applied immediately — no reboot, no unplug.

`READ_ONLY` is honoured live by the SCSI layer, so toggling it takes effect as
soon as the host next checks write permission (e.g. on filesystem remount).

### Notes on `VOLUME_LABEL`

- FAT labels are limited to **11 characters**, no spaces, uppercase suggested.
- Only ASCII is supported reliably.
- The label shown by the OS comes from the boot sector; FatFS rewrites it when
  a new value is parsed.

---

## 2. Compile-time configuration (`src/config.h`)

These are fixed at build time. They cover the things that must be decided
before compiling, such as the flash layout and the GPIO wiring.

### Flash layout

The physical flash size is **detected at boot from the JEDEC ID** (see
`flash_geom_init()` in `lib/fatfs/source/diskio.c`):

| Physical flash | Firmware region | Disk size |
|----------------|-----------------|-----------|
| 2 MB (Pico)    | 512 KB (`0x80000`) | 1.5 MB |
| 16 MB (clone boards) | 1 MB (`0x100000`) | **15 MB** |

`OFFSET_EN_FLASH` / `TAMAÑO_MAXIMO_EN_BYTES` in `config.h` now only define the
**fallback** geometry for a 2 MB flash (used until `flash_geom_init()` runs and
for documentation). Do not raise them above the physical flash size.

### GPIO / I2C / LED

| Macro | Meaning | Default |
|-------|---------|---------|
| `GPIO_LED_ESTADO` | Status LED GPIO | `25` (onboard) |
| `I2C_SDA_PIN` | SSD1306 SDA | `4` |
| `I2C_SCL_PIN` | SSD1306 SCL | `5` |
| `OLED_ADDR` | SSD1306 7-bit I2C address | `0x3C` |
| `OLED_WIDTH` / `OLED_HEIGHT` | Display size | `128` / `64` |

### USB identification

Compile-time only (no `config.txt` key): the USB descriptors are baked into
the firmware, so changing them requires re-compiling and re-flashing. They show
up in `lsusb` / the OS device properties (e.g. "My Companion" on Android).

| Macro | Value | Meaning |
|-------|-------|---------|
| `USB_VID` | `0xCAFE` | Vendor ID |
| `USB_PID` | `0x4000` | Product ID |
| `USB_MANUFACTURER` | `"MyCompany"` | Device manufacturer string |
| `USB_PRODUCT` | `"PicoDrive"` | Device product string |

### Filesystem

| Macro | Value | Meaning |
|-------|-------|---------|
| `FAT_SECTOR_SIZE` | `512` | Logical sector size (must match flash sector access) |

### Runtime defaults (seed the auto-created `config.txt`)

`CFG_DEFAULT_VOLUME_LABEL`, `CFG_DEFAULT_READ_ONLY`,
`CFG_DEFAULT_ENABLE_OLED`, `CFG_DEFAULT_LED_ON_CONNECT`,
`CFG_DEFAULT_AUTO_MOUNT_DELAY_MS`.

### Config watcher

| Macro | Value | Meaning |
|-------|-------|---------|
| `CONFIG_POLL_INTERVAL_MS` | `2000` | How often `config.txt` is re-read for changes |
