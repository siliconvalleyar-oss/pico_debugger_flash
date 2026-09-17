# Raspberry Pi Pico Debug Probe Setup Report

**Date:** 2026-09-17  
**Project Directory:** `/mnt/disk/src/rpico`  
**Goal:** Configure one Raspberry Pi Pico as Debug Probe to program another Pico via SWD



#Firmware Download

https://github.com/raspberrypi/debugprobe/releases/tag/debugprobe-v2.3.1
---
Leer :
https://pip-assets.raspberrypi.com/categories/610-raspberry-pi-pico/documents/RP-008276-DS-2-getting-started-with-pico.pdf



## 1. Executive Summary

Successfully set up a Raspberry Pi Pico as a Debug Probe (CMSIS-DAP) and created a complete blink firmware project with build/program scripts. The Debug Probe firmware is loaded and detected. The target Pico programming via SWD fails due to hardware connection issues (BOOTSEL mode conflict / wiring).

---

## 2. Environment

| Component | Path/Version |
|-----------|--------------|
| Pico SDK | `/mnt/disk/src/rpico/pico-sdk` |
| Pico Examples | `/mnt/disk/src/rpico/pico-examples` |
| OpenOCD (built) | `/mnt/disk/src/rpico/openocd-src/src/openocd` |
| hidapi (built) | `/mnt/disk/src/rpico/hidapi-install` |
| Project Workspace | `/mnt/disk/src/rpico/pico_src/blink` |

---

## 3. Steps Performed

### 3.1 Debug Probe Firmware

**Downloaded pre-built firmware:**
```bash
wget https://github.com/raspberrypi/debugprobe/releases/latest/download/debugprobe.uf2
cp debugprobe.uf2 /media/optimus/RPI-RP2/
```
- ✅ Firmware loaded on Debug Probe Pico
- ✅ Detected as: `VID:PID=0x2e8a:0x000c, serial=E660C0D1C7309C30`
- ✅ Recognized by OpenOCD as CMSIS-DAPv2

**Attempted source build (failed):**
- `picoprobe` repo: Missing `pico/usb_reset.h` in SDK
- `debugprobe` repo: Same error - SDK version mismatch

### 3.2 OpenOCD Build

**Built from source with CMSIS-DAP support:**
```bash
git clone https://github.com/openocd-org/openocd.git
cd openocd-src
git submodule update --init --recursive
# Built hidapi from source
./configure --enable-cmsis-dap --enable-internal-jimtcl --disable-werror
make -j4
```
- ✅ OpenOCD 0.12.0+dev built at `/mnt/disk/src/rpico/openocd-src/src/openocd`
- ✅ Requires `LD_LIBRARY_PATH=/mnt/disk/src/rpico/hidapi-install/lib`

### 3.3 Blink Firmware Project

**Created at `/mnt/disk/src/rpico/pico_src/blink/`:**

| File | Description |
|------|-------------|
| `blink.c` | Blinks onboard LED (GPIO 25) every 500ms |
| `CMakeLists.txt` | CMake config for Pico 1 (RP2040) |
| `pico_sdk_import.cmake` | SDK importer |
| `build.sh` | Builds firmware (generates .uf2 + .elf) |
| `program.sh` | Programs via Debug Probe (SWD) |
| `build_and_program.sh` | Combined build + program |
| `flash_simple.sh` | Build + program with sudo -v |
| `flash_rescue.sh` | Rescue mode attempt |
| `README.md` | Full documentation |

**Build successful:**
```bash
./build.sh
# Outputs: build/blink.uf2 (13KB), build/blink.elf (338KB)
```

---

## 4. SWD Connection Configuration

### Correct Pinout (Pico with debugprobe.uf2 firmware)

| Debug Probe Pico | Target Pico | Signal |
|------------------|-------------|--------|
| **Pin 4 (GP2)** | **Pin 4 (GP2)** | **SWDIO** (Data) |
| **Pin 5 (GP3)** | **Pin 5 (GP3)** | **SWCLK** (Clock) |
| **GND** | **GND** | Ground |
| **Pin 36 (3V3 OUT)** | **Pin 36 (3V3)** | 3.3V Power |

**⚠️ Critical:**
- GP2 = SWDIO (Data), GP3 = SWCLK (Clock) - verified in firmware source
- Pin 36 = 3V3 OUT (not Pin 35 which is ADC_VREF)
- Target Pico MUST NOT have USB connected (exits BOOTSEL mode)

---

## 5. OpenOCD Configuration

**File: `/mnt/disk/src/rpico/debugprobe-openocd.cfg`**
```tcl
adapter driver cmsis-dap
adapter usb vid_pid 0x2e8a 0x000c
adapter speed 5000
transport select swd
source [find target/rp2040.cfg]
init
```

**Rescue mode config created:** `/mnt/disk/src/rpico/test_rescue.cfg`

---

## 6. Errors Encountered

### 6.1 Build Errors

| Error | Cause | Resolution |
|-------|-------|------------|
| `pico/usb_reset.h: No such file` | SDK version mismatch (debugprobe source needs newer SDK) | Used pre-built UF2 instead |
| Assembly errors in boot_stage2 | Host assembler used instead of arm-none-eabi | Fixed by building in clean dir with SDK path |

### 6.2 OpenOCD Errors

| Error | Cause | Status |
|-------|-------|--------|
| `can't find interface/cmsis-dap.cfg` | Missing `-s` script path | Fixed with `-s /mnt/disk/src/rpico/openocd-src/tcl` |
| `could not open device 0x2e8a:0x000c: Access denied` | Missing udev rules | Needs udev rule or sudo |
| `Failed to connect multidrop rp2040.dap0` | Target not responding on SWD | **UNRESOLVED - Hardware issue** |

### 6.3 SWD Connection Failure

**Root Cause Analysis:**
1. **Target Pico in BOOTSEL mode** (USB connected, appears as RPI-RP2) → SWD disabled by hardware
2. **Possible wiring issues** - needs verification with multimeter
3. **Missing 3.3V on target Pin 36** when USB disconnected
4. **GND not shared** between Debug Probe and target

---

## 7. Working Solutions

### 7.1 Method 1: BOOTSEL Drag & Drop (Working)
```bash
# Target Pico: Hold BOOTSEL + connect USB → release BOOTSEL
cp /mnt/disk/src/rpico/pico_src/blink/build/blink.uf2 /media/optimus/RPI-RP2/
```
✅ **Confirmed working** - no Debug Probe needed

### 7.2 Method 2: SWD via Debug Probe (Requires Fix)
**Prerequisites:**
1. Target Pico: **USB DISCONNECTED**
2. Target Pico: **Pin 36 (3V3) connected to Debug Probe Pin 36**
3. SWD wires: **Pin 4↔Pin 4, Pin 5↔Pin 5, GND↔GND**
4. udev rule installed (or use sudo):
```bash
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", ATTR{idProduct}=="000c", MODE="0666"' | sudo tee /etc/udev/rules.d/99-debugprobe.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

**Run:**
```bash
cd /mnt/disk/src/rpico/pico_src/blink
./flash_simple.sh
```

---

## 8. Key Files Created

```
/mnt/disk/src/rpico/
├── debugprobe.uf2                    # Pre-built Debug Probe firmware
├── debugprobe-openocd.cfg           # OpenOCD config for Debug Probe
├── debugprobe-openocd-rescue.cfg    # Rescue mode config
├── test_rescue.cfg                  # Test rescue config
├── test_slow.cfg                    # Slow speed test config
├── openocd-src/                     # Built OpenOCD
├── hidapi-install/                  # Built hidapi library
└── pico_src/blink/                  # Blink project
    ├── blink.c
    ├── CMakeLists.txt
    ├── pico_sdk_import.cmake
    ├── build.sh
    ├── program.sh
    ├── build_and_program.sh
    ├── flash_simple.sh
    ├── flash_rescue.sh
    ├── README.md
    └── build/
        ├── blink.uf2
        ├── blink.elf
        ├── blink.bin
        ├── blink.hex
        └── blink.dis
```

---

## 9. Pending Actions

| Action | Priority |
|--------|----------|
| Verify SWD wiring with multimeter (continuity Pin 4, 5, GND, 3V3) | HIGH |
| Install udev rule for Debug Probe | MEDIUM |
| Test SWD with target Pico USB disconnected + 3V3 from Debug Probe | HIGH |
| If SWD fails: use BOOTSEL method as fallback | LOW |

---

## 10. Quick Reference Commands

```bash
# Build firmware
cd /mnt/disk/src/rpico/pico_src/blink && ./build.sh

# Program via BOOTSEL (target Pico in BOOTSEL mode)
cp build/blink.uf2 /media/optimus/RPI-RP2/

# Program via SWD (target Pico: NO USB, 3V3 from Debug Probe)
cd /mnt/disk/src/rpico/pico_src/blink && ./flash_simple.sh

# Manual OpenOCD
sudo LD_LIBRARY_PATH=/mnt/disk/src/rpico/hidapi-install/lib \
  /mnt/disk/src/rpico/openocd-src/src/openocd \
  -s /mnt/disk/src/rpico/openocd-src/tcl \
  -f /mnt/disk/src/rpico/debugprobe-openocd.cfg \
  -c "program /mnt/disk/src/rpico/pico_src/blink/build/blink.elf verify reset exit"
```

---

## 11. Conclusion

- **Debug Probe:** ✅ Working (firmware loaded, detected by OpenOCD)
- **Firmware Build:** ✅ Working (blink.uf2/blink.elf generated)
- **BOOTSEL Programming:** ✅ Working
- **SWD Programming:** ❌ Blocked by target Pico in BOOTSEL mode / wiring

**Next step:** Disconnect target Pico USB, verify Pin 36 (3V3) powered from Debug Probe, check continuity on all 4 wires, then run `./flash_simple.sh`.
