# SKILL.md - Pendrive USB Configurable (Raspberry Pi Pico RP2040)

Referencia técnica completa del proyecto **pico_usb_drive_configurable**.
Diseñado para que un agente de IA o desarrollador pueda comprender, modificar
y mantener el proyecto de forma autónoma.

---

## 1. Resumen del Proyecto

Pendrive USB persistente basado en Raspberry Pi Pico RP2040 cuyo almacenamiento
vive en la **flash QSPI interna (2 MB..16 MB, tamaño auto-detectado por JEDEC
al arrancar; FAT/exFAT)**. A diferencia de un
ramdisk volátil, los datos **persisten** porque se escriben en la flash SPI del
Pico. Todo es configurable en **tiempo de compilación** (`src/config.h`) y en
**tiempo de ejecución** (`config.txt` en el propio pendrive), con un *Config
Watcher* que detecta cambios cada 2 s sin reiniciar.

- **Lenguaje:** C++17 (núcleo), C (FatFS)
- **SDK:** Pico SDK (CMake 3.13+)
- **Firmware:** USB Mass Storage (MSC) sobre TinyUSB + FatFS + I2C OLED SSD1306
- **Versión actual:** v1.1
- **Licencia:** MIT (FatFS: ChaN, `lib/fatfs/source/License.txt`)
- **Plataforma:** RP2040 (Dual Core ARM Cortex-M0+, 125 MHz)

### Funcionalidades clave

- **Almacenamiento persistente**: exFAT en la flash QSPI interna; 1.5 MB en un
  Pico de 2 MB, **15 MB en una placa de 16 MB** (geometría derivada del JEDEC).
- **Auto-formateo solo si está vacía**: si la región es todo 0xFF/0x00, se
  formatea sola a **exFAT** en el primer arranque (FAT32 es imposible < 32 MB y
  Android no auto-monta FAT16). Un volumen ya existente
  (FAT **o exFAT**, p.ej. formateado por Android) se monta tal cual y **nunca
  se borra** — se elimina el bucle "pide formatear cada vez que lo conectas".
- **Etiqueta de volumen** aplicada desde `config.txt` (`VOLUME_LABEL`).
- **USB MSC**: el Pico se ve como pendrive normal (R/W).
- **Config runtime** (`config.txt`): `VOLUME_LABEL`, `READ_ONLY`,
  `ENABLE_OLED`, `LED_ON_CONNECT`, `AUTO_MOUNT_DELAY_MS`.
- **Config Watcher** (hot-plug): re-lee `config.txt` cada 2 s comparando un
  hash; aplica cambios sin reboot.
- **Auto-creación**: en el primer arranque escribe `config.txt` con los
  valores por defecto.
- **OLED SSD1306 128x64**: etiqueta del volumen, candado read-only, estado USB,
  capacidad total/libre y error de config.
- **LED de estado** con patrones no bloqueantes.
- **Robusto ante corte de luz**: `fatfs_sync()` / flush de buffers FAT.

---

## 2. Arquitectura del Firmware

```
┌────────────────────────────────────────────────────────────┐
│                          main.cpp                           │
│  orquestador: arranque, mount FS, USB, hot-plug watcher     │
│                                                              │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │config_manage│  │fatfs_interface│  │   usb_storage      │  │
│  │r (config.txt│  │(mount/format/ │  │ (TinyUSB MSC cbs)  │  │
│  │ + watcher)  │  │  sync)        │  └─────────┬──────────┘  │
│  └──────┬──────┘  └───────┬──────┘            │              │
│         │                 │                    │ disk_read/   │
│         │ g_cfg           │ f_*                │ disk_write   │
│  ┌──────▼──────┐  ┌───────▼──────┐  ┌─────────▼──────────┐  │
│  │ oled_display │  │   lib/fatfs  │  │  lib/fatfs/diskio.c │  │
│  │ (SSD1306)    │  │  (Elm-chan)  │  │ (flash QSPI I/O)   │  │
│  └──────┬──────┘  └───────────────┘  └────────────────────┘  │
│         │ I2C0 (400kHz)                                       │
│  ┌──────▼──────┐                                              │
│  │  OLED SDA 16│  USB: gadgets BMC / MSC (R/W, hot)          │
│  │  OLED SCL 17│  LED estado: GPIO 25                        │
│  └─────────────┘                                              │
└──────────────────────────────────────────────────────────────┘
```

### Bucle principal (`main.cpp`, `while(1)`)

| Paso | Función | Frecuencia |
|------|---------|------------|
| 1 | `tud_task()` — procesa USB (MSC) | Cada iteración |
| 2 | `config_manager_poll()` — watcher hot-plug | Cada `CONFIG_POLL_INTERVAL_MS` (2 s) |
| 3 | `gpio_led_task()` — patrones LED | Cada iteración |
| 4 | `oled_render()` — refresca el OLED | Cada iteración (si `ENABLE_OLED`) |

### Orden de arranque (`main()`)

1. `gpio_led_init()` + fast blink (boot/init).
2. `fatfs_mount()` → monta la FAT (formatea si hace falta).
3. `config_manager_init()` → carga `config.txt` (o lo crea).
4. `fatfs_get_free_mb()` → llena `g_state.total_mb/free_mb`.
5. `oled_init()` + `oled_render()` (si `ENABLE_OLED`).
6. `usb_device_init()` → inicializa TinyUSB.
7. Delay `AUTO_MOUNT_DELAY_MS` para que el FS esté listo antes de enumerar.
8. `while(1)` como la tabla anterior.

---

## 3. Mapa de Archivos Detallado

```
pico_src/pico_usb_drive_configurable/
├── CMakeLists.txt               # Build raíz: SDK + fatfs + src
├── pico_sdk_import.cmake        # Importador del SDK (del SDK, no modificar)
├── build/                       # Salida de compilación (ignorada por git)
│   └── src/
│       └── pico_usb_drive_configurable.uf2   # Firmware a flashear
├── src/
│   ├── CMakeLists.txt           # Build del ejecutable + librerías
│   ├── config.h                 # ★ TODA la config compile-time (con rangos)
│   ├── main.cpp                 # ★ Orquestador + descriptores USB
│   ├── config_manager.cpp       # ★ config.txt read/write + Config Watcher
│   ├── fatfs_interface.cpp      # mount / format / sync del FS FAT
│   ├── usb_storage.cpp          # ★ Callbacks TinyUSB MSC → flash sector
│   ├── oled_display.cpp         # Driver SSD1306 + render de estado
│   ├── gpio_control.cpp         # Patrones de LED no bloqueantes
│   └── tusb_config.h            # TinyUSB: solo MSC habilitado
├── include/
│   ├── pendrive.h               # ★ Estado compartido (g_cfg, g_state) + prototipos
│   ├── config_manager.h
│   ├── fatfs_interface.h
│   ├── usb_storage.h
│   ├── oled_display.h
│   └── gpio_control.h
├── lib/fatfs/
│   ├── CMakeLists.txt           # Build de la librería estática "fatfs"
│   ├── LICENSE.txt              # Licencia FatFS (Elm-chan / ChaN)
│   └── source/
│       ├── ff.c ff.h            # Núcleo FatFS
│       ├── ffunicode.c          # Soportes Unicode
│       ├── ffsystem.c           # Capa OS (none → no-ops)
│       ├── diskio.c diskio.h    # ★ disk I/O sobre la flash (sobrescrito)
│       ├── ffconf.h             # Opciones de compilación FatFS
│       ├── 00history.txt
│       └── 00readme.txt
└── docs/
    ├── README.md                # Índice general
    ├── BUILD_INSTRUCTIONS.md    # Cómo compilar y flashear
    ├── CONFIGURATION_GUIDE.md   # Todas las opciones (runtime + compile)
    ├── HARDWARE_SETUP.md        # Cableado OLED / LED / flash
    ├── API_REFERENCE.md         # API interna de módulos
    ├── TROUBLESHOOTING.md       # Problemas comunes y soluciones
    └── SKILL.md                 # ★ Este documento
```

---

## 4. layout de Flash (auto-detectado por JEDEC: 2 MB..16 MB)

La geometría real se calcula en el arranque por `flash_geom_init()` (en
`lib/fatfs/source/diskio.c`), que lee el ID JEDEC con `flash_do_cmd()`:

| Flash física | Región firmware | Disco |
|--------------|-----------------|-------|
| 2 MB (Pico oficial) | `< 512 KB` | 1.5 MB (`0x80000..0x200000`) |
| 16 MB (placas clon)  | `< 1 MB`   | **15 MB** (`0x100000..0x1000000`) |

```
Dirección física     2 MB                 16 MB
──────────────────────────────────────────────────
0x000000 ┐
         │ Firmware   < 0x080000           < 0x100000
─────────┼────────────────────────────────────────
         │ Disco FAT  0x080000..0x1FFFFF   0x100000..0xFFFFFF
```

| Macro | Valor | Descripción |
|-------|-------|-------------|
| `OFFSET_EN_FLASH` | `0x80000` | Fallback inicio de la zona FAT (2 MB) |
| `OFFSET_EN_FLASH_16MB` | `0x100000` | Inicio de la zona FAT para flash > 4 MB |
| `TAMAÑO_MAXIMO_EN_BYTES` | `0x180000` (1.5 MB) | Fallback tamaño del disco (2 MB) |
| `DISK_SECTOR_SIZE` | `512` | Sector lógico |
| `DISK_SECTOR_COUNT` | `TAMAÑO_MAXIMO_EN_BYTES / 512` | Sectores (solo fallback) |

`g_geom` (`pendrive_geom_t {flash_size, disk_offset, disk_size}`) contiene la
geometría en runtime y es lo que usa realmente `disk_read/write`, `usb_storage`
(capacidad MSC) y la guarda de auto-formateo.

> ⚠️ Los macros de `config.h` son SOLO el fallback para un Pico de 2 MB. No
> subir los tamaños por encima de la flash física. Nuestro firmware (~66 KB)
> cabe holgado tanto en 512 KB como en 1 MB.

---

## 5. Estado Compartido (`include/pendrive.h`)

`g_cfg` (config runtime), `g_state` (estado en vivo) y `g_geom` (geometría de
flash) son globales visibles a todos los módulos:

```c
typedef struct {
    char      volume_label[12]; // VOLUME_LABEL
    uint8_t   read_only;        // READ_ONLY (0/1)
    uint8_t   enable_oled;      // ENABLE_OLED (0/1)
    uint8_t   led_on_connect;   // LED_ON_CONNECT (0/1)
    uint32_t  auto_mount_delay_ms; // AUTO_MOUNT_DELAY_MS
    bool      config_valid;     // false si config.txt tiene errores
} pendrive_cfg_t;

typedef struct {
    bool  mounted, reading, writing;
    uint32_t total_mb, free_mb;
} pendrive_state_t;

typedef struct {
    uint32_t flash_size;  uint32_t disk_offset;  uint32_t disk_size;
} pendrive_geom_t;   // extern: g_geom (inicializado por flash_geom_init)
```

---

## 6. Módulos

### 6.1 `config_manager` (config.txt + Config Watcher)

- `config_manager_init()`: carga `config.txt` (lo crea con defaults si falta).
- `config_manager_poll()`: watcher hot-plug; re-lee el archivo cada 2 s y
  compara un **hash FNV-1a** del contenido; si cambió, re-aplica `g_cfg`.

Parsea líneas `KEY=VALUE`, tolera comentarios `#`, espacios y claves
case-insensitives. Claves desconocidas o valores inválidos → `config_valid=false`
(muestra `config.txt: error` en el OLED).

### 6.2 `fatfs_interface` (FAT en flash)

- `fatfs_mount()`: `f_mount` + probe con `f_getfree`; si `FR_NO_FILESYSTEM`
  **y** `flash_region_is_blank()` (región toda 0xFF/0x00), hace
  `f_mkfs("", {FM_EXFAT}, ...)` una vez (exFAT; Android lo monta sin preguntar).
  Si la región contiene datos no reconocidos devuelve `-2` y **no borra nada**.
- `fatfs_set_label()`: aplica `VOLUME_LABEL` con `f_setlabel` (FAT y exFAT).
- `fatfs_get_free_mb()`: cálculo desde `f_getfree` (`n_fatent`, `csize`, 512 B).
  **Ojo:** `ssize` no existe porque `FF_MIN_SS == FF_MAX_SS == 512`; usar `512`.
- `fatfs_sync()`: flushes buffers FAT (abre/cierra la raíz) para robustez.

### 6.3 `usb_storage` (TinyUSB MSC)

Callbacks extern "C" que traducen SCSI READ10/WRITE10 a `disk_read`/`disk_write`
di la flash:

- `tud_msc_capacity_cb` → `DISK_SECTOR_COUNT` / `DISK_SECTOR_SIZE`.
- `tud_msc_read10_cb` / `write10_cb` → lee/escribe sectores (read-modify-write
  para sub-sector).
- `tud_msc_is_writable_cb` → `!g_cfg.read_only` (hot-appliable).
- Se **usa `diskio.h`** para `LBA_t`, `RES_OK`, etc. (incluir `ff.h` antes).

### 6.4 `oled_display` (SSD1306 128x64)

- **Driver alineado con el oficial `pico-examples/i2c/ssd1306_i2c`**:
  - `oled_send_cmd`: control byte `0x80` + cmd (un solo `i2c_write_blocking`).
  - `oled_send_data`: control byte `0x40` + payload en **UNA** transacción I2C
    (¡no fragmentar en chunks → causaba "lluvia"/ruido!).
  - Init secuencia oficial (COM pins `0x12` para 128x64, contraste `0xFF`,
    precharge `0xF1`, VCOM `0x30`, desactivar scroll `0x2E`).
  - Modo **horizontal** (`0x20, 0x00`) con `SET_COL_ADDR(0x21)` +
    `SET_PAGE_ADDR(0x22)` + bulk del framebuffer lineal.
- Framebuffer lineal `fb[HEIGHT/8][WIDTH]` (`[8][128]`), indexing
  `(y/8)*WIDTH + x`, bit `1 << (y%8)` — idéntico a `SetPixel` oficial.
- Pines: SDA=GPIO16, SCL=GPIO17 (I2C0). Dirección `0x3C`.
- `oled_render(cfg, st)`: dibuja label + candado, estado USB, Total/Libre MB,
  y error de config. `ENABLE_OLED=0` → panel en sleep.

### 6.5 `gpio_control` (LED)

Patrones no bloqueantes vía `gpio_led_task()` (timebase `time_us_64()`):
- SOLID: montado/OK.
- FAST (~10 Hz): init/formateo/error.
- SLOW (~1 Hz): error de `config.txt`.
- OFF: `LED_ON_CONNECT=0` sin actividad.

---

## 7. Flujo de datos de escritura (R/W)

```
Host (PC) ──SCSI WRITE10──► tud_msc_write10_cb (usb_storage.cpp)
                              │  respeta g_cfg.read_only
                              ▼
                          disk_write (diskio.c)
                              │  lee bloque 4 KB actual (XIP-safe)
                              │  superpone el/los sector(es)
                              │  save_and_disable_interrupts()
                              │  flash_range_erase(bloque 4 KB)
                              │  flash_range_program(bloque 4 KB)
                              │  restore_interrupts()
                              ▼
                         flash QSPI (persistente)
```

Lectura análoga con `disk_read` (memcpy directo XIP).

---

## 8. Configuración

Ver `docs/CONFIGURATION_GUIDE.md` para la lista completa.

**Runtime (`config.txt`)** — aplicado en vivo:

| Clave | Valores | Defecto |
|-------|---------|---------|
| `VOLUME_LABEL` | 1..11 chars | `MiPendrive` |
| `READ_ONLY` | 0/1 | `0` |
| `ENABLE_OLED` | 0/1 | `1` |
| `LED_ON_CONNECT` | 0/1 | `1` |
| `AUTO_MOUNT_DELAY_MS` | 0..60000 | `500` |

**Compile-time (`src/config.h`)** — layout flash, GPIO/I2C, USB IDs, defaults.

---

## 9. Commandos útiles

```bash
# Compilar
cd pico_usb_drive_configurable
mkdir -p build && cd build
cmake ..                 # o -DPICO_SDK_PATH=/ruta/al/sdk
make -j"$(nproc)"

# Flashear (BOOTSEL)
# copiar build/src/pico_usb_drive_configurable.uf2 al RPI-RP2

# Verificar montaje RW en Linux
lsblk
sudo dmesg | tail        # debe montar rw (no "invalid start cluster")
# si sale read-only: reformatear con sudo mkfs.vfat -F 16 <dev>
```

---

## 10. Problemas Conocidos y Soluciones

| Síntoma | Causa | Solución |
|---------|-------|----------|
| Montaje read-only en Linux | FAT corrupto (`invalid start cluster`) | Ver `TROUBLESHOOTING.md`; reformatear. |
| OLED con "lluvia"/ruido | Burst de datos I2C fragmentado | Enviar 0x40+payload en una sola transacción. |
| OLED descuadrado | Init/write en modo de memoria inconsistente | Usar modo horizontal + ventana 0x21/0x22. |
| Config no aplica | Watcher cada 2 s; READ_ONLY necesita remount | Esperar, o remontar el volumen. |

---

## 11. Notas de Mantenimiento

- **FatFS `ffconf.h`**: ya habilitado `FF_USE_MKFS=1`, `FF_USE_LABEL=1`,
  `FF_USE_STRFUNC=2`, `FF_USE_LFN=1`. No cambiar `FF_MIN_SS`/`FF_MAX_SS`
  (fijadas a 512) sin ajustar el uso de `ssize` en `fatfs_interface.cpp`.
- **`diskio.c`** es C puro; al incluir sus símbolos desde C++ usar `ff.h`
  (tiene `extern "C"`). `diskio.h` NO trae `ff.h`: incluirlo antes.
- **flash en vivo**: `flash_range_erase/program` son XIP-safe y bloquean
  interrupciones brevemente → pausa el USB durante escrituras pequeñas.
- **Rama git**: `configurable_usb_drive` (`origin/configurable_usb_drive`),
  en el repo `siliconvalleyar-oss/pico_src`.
- Para futuros cambios, correr el build completo y verificar 0 warnings.

---

## 12. Referencias

- `pico-examples/i2c/ssd1306_i2c/ssd1306_i2c.c` — driver OLED oficial (referencia).
- FatFS: http://elm-chan.org/fsw/ff/00index_e.html
- TinyUSB MSC: `pico-sdk/lib/tinyusb/src/device/usbd_pvt.h` + clases.
- Pico SDK flash: `hardware/flash.h`.
