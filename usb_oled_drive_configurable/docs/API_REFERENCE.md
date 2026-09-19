# API Reference

This document describes the internal module API of the firmware. All symbols
are declared in `include/*.h` and implemented in `src/*.cpp`. Because the code
is a mix of C++ and C (FatFS), the public module headers are wrapped with
`extern "C"`.

---

## Shared state — `include/pendrive.h`

### `pendrive_cfg_t`
Runtime configuration struct populated from `config.txt`.

| Field | Type | Source key |
|-------|------|-----------|
| `volume_label[12]` | `char[]` | `VOLUME_LABEL` |
| `read_only` | `uint8_t` | `READ_ONLY` |
| `enable_oled` | `uint8_t` | `ENABLE_OLED` |
| `led_on_connect` | `uint8_t` | `LED_ON_CONNECT` |
| `auto_mount_delay_ms` | `uint32_t` | `AUTO_MOUNT_DELAY_MS` |
| `config_valid` | `bool` | (derived) false on syntax error |

### `pendrive_state_t`
Live system state used by the OLED.

| Field | Type | Meaning |
|-------|------|---------|
| `mounted` | `bool` | Host mounted our MSC LUN |
| `reading` / `writing` | `bool` | An MSC READ/WRITE in progress |
| `total_mb` / `free_mb` | `uint32_t` | Capacity in MB |

### Globals
```c
extern pendrive_cfg_t   g_cfg;
extern pendrive_state_t g_state;
```

---

## `config_manager` — `include/config_manager.h`, `src/config_manager.cpp`

| Function | Description |
|----------|-------------|
| `void config_manager_init(void)` | Load `config.txt`; auto-create it with defaults on first boot. |
| `void config_manager_poll(void)` | Hot-plug watcher; call every `CONFIG_POLL_INTERVAL_MS`. Re-applies changes on content-hash mismatch. |

Supports `#` comments, whitespace, and case-insensitive keys. Unknown keys or
invalid values clear `g_cfg.config_valid`.

---

## `fatfs_interface` — `include/fatfs_interface.h`, `src/fatfs_interface.cpp`

| Function | Description |
|----------|-------------|
| `int fatfs_mount(void)` | Mount the FAT volume; formats it (FAT) once if no filesystem is present. Returns `0` on success. |
| `int fatfs_get_free_mb(uint32_t *free_mb, uint32_t *total_mb)` | Fill capacity in MB from `f_getfree`. |
| `void fatfs_sync(void)` | Flush dirty FAT buffers (robustness against power loss). |

---

## `usb_storage` — `include/usb_storage.h`, `src/usb_storage.cpp`

| Function | Description |
|----------|-------------|
| `void usb_storage_init(void)` | Install / prepare the MSC role. |

TinyUSB MSC callbacks (C linkage) translate SCSI READ10/WRITE10 to
`disk_read()`/`disk_write()` on the flash region:

| Callback | Role |
|----------|------|
| `tud_msc_inquiry_cb` | Identity strings |
| `tud_msc_capacity_cb` | Block count / size |
| `tud_msc_read10_cb` | Sector read from flash |
| `tud_msc_write10_cb` | Sector write to flash (honours `READ_ONLY`) |
| `tud_msc_is_writable_cb` | Returns `!g_cfg.read_only` (hot-applied) |
| `tud_msc_test_unit_ready_cb` / `tud_msc_start_stop_cb` / `tud_msc_scsi_cb` / `tud_msc_synchronize_cache_cb` | Bookkeeping / sense |

---

## `oled_display` — `include/oled_display.h`, `src/oled_display.cpp`

| Function | Description |
|----------|-------------|
| `void oled_init(void)` | Init I2C + SSD1306 (raw driver). |
| `void oled_render(const pendrive_cfg_t *cfg, const pendrive_state_t *st)` | Render status (label, lock, USB state, capacity, config-error). Turning `ENABLE_OLED=0` puts the panel to sleep. |

Also exposes the low-level framebuffer + 5x7 font.

---

## `gpio_control` — `include/gpio_control.h`, `src/gpio_control.cpp`

| Function | Description |
|----------|-------------|
| `void gpio_led_init(void)` | Set the LED GPIO as output, off. |
| `void gpio_led_set_solid(void)` | Mounted / OK. |
| `void gpio_led_set_fast_blink(void)` | Init / formatting / error. |
| `void gpio_led_set_slow_blink(void)` | `config.txt` syntax error. |
| `void gpio_led_set_off(void)` | Off (respects `LED_ON_CONNECT=0`). |
| `void gpio_led_task(void)` | Non-blocking pattern tick; call often. |

---

## FatFS low-level — `lib/fatfs/source/diskio.c`

Flash-backed disk interface overrides the stock FatFS disk I/O:

| Function | Description |
|----------|-------------|
| `disk_initialize` / `disk_status` | Always ready, writable. |
| `disk_read` | XIP-safe `memcpy` from flash. |
| `disk_write` | Read-modify-erase-program a whole 4 KB flash block. |
| `disk_ioctl` | `CTRL_SYNC`, `GET_SECTOR_COUNT`, `GET_SECTOR_SIZE`, `GET_BLOCK_SIZE`. |
