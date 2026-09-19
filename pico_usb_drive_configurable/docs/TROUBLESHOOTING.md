# Troubleshooting

## The drive mounts read-only on Linux

This is the classic symptom of a **corrupted FAT filesystem** — the kernel
detects an invalid cluster and flips the mount to read-only. It is *not* a
hardware write-protect issue (SCSI reports `Write Protect is off`).

The known root cause this project was designed to avoid: a FAT directory entry
with an **endianness-inverted or misplaced cluster start field**. The kernel
then sees e.g. `invalid start cluster (start 00000200)` instead of `00000002`.

Fixes / checks:

- If your drive was written by an older (buggy) firmware, back up and
  reformat: unmount, then `sudo mkfs.vfat -F 16 <dev>` and remount.
- Our current firmware formats the volume cleanly on first boot, so a fresh
  drive should mount rw out of the box.
- Check `dmesg` for `error, fat_get_cluster: invalid start cluster`.

## The computer does not see the pendrive at all

- Make sure you used a **data** USB cable, not a charge-only one.
- Check `lsusb` shows `Cafe:4000` (or your custom `USB_VID`/`USB_PID`).
- Confirm the firmware flashed successfully (the `RPI-RP2` drive during
  BOOTSEL is separate and not the pendrive).

## Files appear to be lost after power-off

- Make sure the pendrive is **safely unmounted / ejected** before unplugging,
  like any USB drive.
- `fatfs_sync()` flushes on close/sync, which protects against most abrupt
  disconnects, but a write in progress at power-cut can still lose data.

## The OLED is blank or shows nothing

- `ENABLE_OLED=1` must be set in `config.txt`.
- Verify wiring (see `docs/HARDWARE_SETUP.md`) and the I2C address (`0x3C`).
- If `config.txt` has a syntax error, the OLED shows `config.txt: error`.

## LED behaviour

- **Solid**: mounted and OK.
- **Fast blink**: boot / init / formatting / error.
- **Slow blink**: `config.txt` is invalid.
- **Off**: `LED_ON_CONNECT=0` and no activity.

## I edited `config.txt` but nothing changed

- Wait up to 2 s (the hot-plug watcher poll interval).
- For `READ_ONLY` the host must re-check write permission; remount the volume
  or unplug/replug.
- Ensure the file was saved with the exact key names (see
  `docs/CONFIGURATION_GUIDE.md`).

## Rebuilding after changing `config.h`

Always reconfigure when you edit `CMakeLists.txt`:
`cmake ..` in the build dir, then `make`.
