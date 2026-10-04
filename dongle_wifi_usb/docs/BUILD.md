# Build Guide - dongle_wifi_usb

## Requisitos Previos

### Hardware
- Raspberry Pi Pico W (RP2040 + CYW43439)
- Cable USB micro-B
- Opcional: OLED SSD1306 128x64 I2C (GP4=SDA, GP5=SCL)

### Software (Ubuntu/Debian)
```bash
# Toolchain ARM
sudo apt update && sudo apt install -y \
    cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
    build-essential git python3

# pico-sdk (clonar recursivamente para submodulos)
git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init --recursive
export PICO_SDK_PATH=$(pwd)

# openocd (para debug SWD opcional)
# Ver scripts/flash_nosudo_multi.sh
```

### Verificar Instalacion
```bash
arm-none-eabi-gcc --version
# Debe mostrar 13.2.1 o similar

ls $PICO_SDK_PATH
# Debe mostrar: src, lib, tools, CMakeLists.txt, etc.

ls $PICO_SDK_PATH/lib/lwip
# Debe existir (submodulo lwIP)

ls $PICO_SDK_PATH/lib/tinyusb
# Debe existir (submodulo TinyUSB)

ls $PICO_SDK_PATH/lib/cyw43-driver
# Debe existir (submodulo cyw43)
```

---

## Compilacion

### Opcion 1: Script Automatizado (Recomendado)
```bash
cd dongle_wifi_usb
./build.sh
```

### Opcion 2: Manual con CMake
```bash
cd dongle_wifi_usb
mkdir -p build && cd build

export PICO_SDK_PATH=/ruta/a/pico-sdk  # Si no esta en ENV

cmake -DPICO_BOARD=pico_w \
      -DCMAKE_BUILD_TYPE=Release \
      ..

cmake --build . -j$(nproc)
```

### Opcion 3: Debug Build
```bash
cd dongle_wifi_usb/build
cmake -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)
```

### Salida Esperada
```
=== Building dongle_wifi_usb ===
Project: /mnt/disk/src/rpico/pico_debugger_flash/dongle_wifi_usb
Build:   /mnt/disk/src/rpico/pico_debugger_flash/dongle_wifi_usb/build

-- PICO_SDK_PATH is /mnt/disk/src/rpico/pico-sdk
-- TinyUSB available at .../lib/tinyusb/hw/bsp/rp2040
-- lwIP available at .../lib/lwip
-- Pico W Wi-Fi build support available.
-- Configuring done
-- Generating done
-- Build files have been written to: ...

[100%] Built target dongle_wifi_usb

=== Build complete ===
UF2: /mnt/disk/src/rpico/pico_debugger_flash/dongle_wifi_usb/build/dongle_wifi_usb.uf2
```

### Archivos Generados
```
build/
├── dongle_wifi_usb.elf      # ELF con simbolos (para debug)
├── dongle_wifi_usb.uf2      # UF2 para flasheo (drag & drop)
├── dongle_wifi_usb.hex      # Intel HEX
├── dongle_wifi_usb.bin      # Binary raw
├── dongle_wifi_usb.map      # Memory map
└── dongle_wifi_usb.dis      # Disassembly
```

---

## Flasheo

### Metodo 1: Modo BOOTSEL (Drag & Drop)
1. Desconectar Pico
2. Mantener boton **BOOTSEL** presionado
3. Conectar USB a la PC
4. Aparece unidad `RPI-RP2`
5. Copiar UF2:
   ```bash
   cp build/dongle_wifi_usb.uf2 /media/$USER/RPI-RP2/
   # O en Windows: copy build\dongle_wifi_usb.uf2 E:\
   ```
6. Pico se reinicia automaticamente

### Metodo 2: Via Raspberry Pi Remota (SSH)
```bash
# En PC: compilar y copiar UF2
scp build/dongle_wifi_usb.uf2 joy@raspberry.local:/home/joy/

# En Raspberry Pi: actualizar repo y flashear
ssh joy@raspberry.local "
  cd /home/joy/src/pico/pico_debugger_flash && git pull
  # Poner Pico en modo BOOTSEL y copiar
  cp /home/joy/dongle_wifi_usb.uf2 /media/joy/RPI-RP2/
"
```

### Metodo 3: SWD/JTAG (openocd)
```bash
# Requiere debug probe (otro Pico o J-Link)
cd /mnt/disk/src/rpico/pico_debugger_flash
BOARD=pico_w ./scripts/flash_nosudo_multi.sh dongle_wifi_usb
```

---

## Verificacion Post-Flasheo

### 1. Verificar en Host Linux
```bash
# Ver dispositivo USB
lsusb | grep -i pico
# Bus 001 Device 005: ID 2e8a:000a Raspberry Pi Pico WiFi Dongle

# Ver interfaz de red
ip addr show
# Debe aparecer usb0 o enx... con IP asignada por DHCP

# Ver dmesg
dmesg -T | grep -i -e rndis -e cdc -e usb
```

### 2. Verificar en Host Windows
```powershell
# Administrador de dispositivos -> Adaptadores de red
# Debe aparecer "RNDIS/Ethernet Gadget" o similar

# ipconfig
# Ver adaptador "RNDIS" con IP
```

### 3. Verificar en Host macOS
```bash
# System Preferences -> Network
# Debe aparecer "RNDIS/Ethernet Gadget" o "USB Ethernet"
```

### 4. LEDs y OLED
- **LED onboard**: Parpadeo rapido (200ms) = Sin USB / Parpadeo lento (1s) = USB conectado
- **OLED**: Debe mostrar dashboard con estado

---

## Troubleshooting Build

### Error: "pico-sdk not found"
```bash
export PICO_SDK_PATH=/ruta/completa/a/pico-sdk
# O editar CMakeLists.txt y cambiar pico_sdk_import.cmake
```

### Error: "lwIP submodule not initialized"
```bash
cd $PICO_SDK_PATH
git submodule update --init lib/lwip
git submodule update --init lib/tinyusb
git submodule update --init lib/cyw43-driver
```

### Error: "CFG_TUD_OS redefined"
- Revisar `include/tusb_config.h` - no definir `CFG_TUSB_OS`

### Error: "arch/cc.h not found"
- Usar `pico_cyw43_arch_lwip_poll` en CMakeLists.txt (incluye lwIP arch)

### Error: "pico_get_random undefined"
- Agregar `pico_rand` a `target_link_libraries`
- Cambiar `LWIP_RAND` a `get_rand_32` en lwipopts.h

### Error: "rndis_class_set_handler undefined"
- Agregar `${PICO_TINYUSB_PATH}/lib/networking/rndis_reports.c` a SOURCES

### Error: "tud_descriptor_* undefined"
- Implementar callbacks en `main.c` con `__attribute__((used))`

### Warning: "stdio USB configured with TinyUSB but CDC not enabled"
- Agregar `CFG_TUD_CDC 1` y buffers en `tusb_config.h`

### Error: "ISO C forbids conversion of function pointer"
- Remover `-Wpedantic` de `add_compile_options` en CMakeLists.txt

---

## Limpieza
```bash
# Limpiar build
cd dongle_wifi_usb
rm -rf build

# O solo recompilar
cd build && cmake --build . --clean-first
```

---

## Variables de Entorno Utiles
```bash
export PICO_SDK_PATH=/home/user/pico-sdk
export PICO_BOARD=pico_w
export PICO_PLATFORM=rp2040
# Para builds paralelos
export MAKEFLAGS="-j$(nproc)"
```

---

## Cross-Compilation desde Docker
```dockerfile
FROM ubuntu:22.04
RUN apt update && apt install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential git python3
WORKDIR /workspace
RUN git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
ENV PICO_SDK_PATH=/workspace/pico-sdk
RUN cd pico-sdk && git submodule update --init --recursive
COPY dongle_wifi_usb ./dongle_wifi_usb
RUN cd dongle_wifi_usb && ./build.sh
```

---

## CI/CD (GitHub Actions Example)
```yaml
name: Build
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Install deps
        run: sudo apt update && sudo apt install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi
      - name: Setup pico-sdk
        run: |
          git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
          echo "PICO_SDK_PATH=$(pwd)/pico-sdk" >> $GITHUB_ENV
      - name: Build
        run: |
          cd dongle_wifi_usb
          ./build.sh
      - name: Upload UF2
        uses: actions/upload-artifact@v4
        with:
          name: dongle_wifi_usb.uf2
          path: dongle_wifi_usb/build/dongle_wifi_usb.uf2
```

---

*Documento actualizado: 2026-10-04*