# Qué es y cómo se hizo válido para Android

Este documento explica, en orden, los dos temas centrales del proyecto:

1. **Qué hay físicamente**: dónde vive el "pendrive", si la flash es real y
   cuál es su capacidad exacta.
2. **Qué se hizo para que Android lo tome como un pendrive válido** (deje de
   pedir "formatear" en cada conexión).

---

## 1. El "pendrive" es REAL, no es una simulación

### Dónde viven los datos

Los datos del pendrive se guardan en la **flash QSPI interna del Pico**
(soldada en la placa), la misma de la que arranca el firmware. No hay RAM
volátil ni emulación: lo que copias al pendrive **se escribe en la flash
física** y persiste tras apagar la placa.

El firmware usa `flash_range_erase()` / `flash_range_program()` del Pico SDK
(vía XIP) para escribir sectores reales de flash. En modo XPIPX (XIP) la flash
se lee por el direccionamiento XIP del RP2040; la escritura se hace con el
comando estándar de programación de la flash. Es el mismo mecanismo que usa
tu SO para leer/escribir cualquier stick USB.

### La flash es REAL y de 16 MiB

La "placa de 16 MB" monta un chip **BY25Q128AS** de Boya (W25Q128-compatible):

```
JEDEC ID devuelto por la flash : 0x184068
Manufacturer      = 0x18 (Boya)
Device 0x40 0x68  = BY25Q128AS
Capacidad         = 128 Mbit = 16 MiB = 16.777.216 bytes
```

Ese JEDEC se lee **en caliente al arrancar** (`flash_geom_init()` en
`lib/fatfs/source/diskio.c`): el firmware manda el comando `0x9F` por QSPI
y calcula el tamaño con `(1u << (cap - 0x14u)) * 1 MiB`.

### Capacidad real que ve el host: 15 MiB (30720 sectores × 512 B)

De los 16 MiB físicos, **1 MiB (0x100000) está reservado para el firmware**
(programa y rutinas de arranque). El resto es el disco expuesto por USB:

```
flash_size   = 16 MiB  = 0x1000000          (detectado por JEDEC)
disk_offset  =  1 MiB  = 0x100000            (firmware)
disk_size    = 15 MiB  = 0x1000000 - 0x100000 = 0xF00000
             = 15.728.640 bytes = 30.720 sectores lógicos de 512 B
```

La capacidad que el host ve por USB-MSC (`READ CAPACITY` del protocolo SCSI)
es exactamente `disk_size / 512 = 30720` sectores → **15 MiB**.

| Dato | Valor |
|---|---|
| Chip de flash | BY25Q128AS (Boya) |
| JEDEC ID | `0x184068` |
| Flash física | 16 MiB = 16.777.216 B |
| Reservado para firmware | 1 MiB (`0x000000..0x0FFFFF`) |
| Disco USB (FAT/exFAT) | **15 MiB** = `0x100000..0x1000000` = 30.720 sectores |

Nota: en un Pico de **2 MiB** el mismo firmware detecta el JEDEC y expone
**1,5 MiB** (512 KiB reservados). La geometría es dinámica; un solo binario
sirve para todas las placas.

---

## 2. Por qué Android pedía "formatear" y cómo se arregló

### Síntoma original

Al conectar el Pico a un móvil Android, el dispositivo se detectaba
("My Companion" / "MiPendrive") pero Android mostraba un diálogo pidiendo
**formatear la unidad**. Esto ocurre porque el **vold** de Android (el
gestor de almacenamiento) solo auto-monta sin preguntar los filesystems que
reconoce de fábrica: **FAT32 y exFAT**. Cuando no puede montar lo que hay en
el disco, ofrece formatearlo.

### Cadena de causas

1. El firmware formateaba la región **en cada arranque** sin comprobar si ya
   había datos → cualquier volumen creado por Android se borraba al reiniciar.
2. El volumen que sí dejaba era **FAT16** (por elección de `f_mkfs` con
   `FM_FAT|FM_FAT32` a < 32 MiB). FAT16 no lo auto-monta el vold de Android
   moderno (solo FAT32/exFAT) → dialog de "formatear".
3. El firmware antiguo carecía de soporte de **lectura exFAT** → si Android
   llegaba a formatearlo a exFAT, el firmware no podía leerlo y volvía a
   marcarlo como desconocido.

### Solución aplicada (por capas)

**A. Nunca se borra un volumen existente** (`src/fatfs_interface.cpp`):

```
fatfs_mount():
  probe con f_getfree()
  si FR_NO_FILESYSTEM:
      si !flash_region_is_blank()  → devuelve -2 y NO toca nada
      (la región con datos no se borra jamás)
      si está en blanco → f_mkfs() UNA vez
```

El guard `flash_region_is_blank()` escanea toda la región de disco (en pasos
de 4 KiB): solo si está 100 % en `0xFF/0x00` (erased) se formatea. Esto rompe
el bucle "Android formatea → el firmware lo destruye → Android vuelve a pedir
formatear".

**B. Soporte de lectura exFAT** (`lib/fatfs/source/ffconf.h`):

```
#define FF_FS_EXFAT 1
```

Con esto FatFS lee **cualquier** volumen de 12/16/32/exFAT que Android (o el
PC) haya escrito, y lo monta tal cual. El firmware jamás reformatea un
volumen que ya existe.

**C. El volumen inicial se crea como exFAT, no FAT16**
(`src/fatfs_interface.cpp`):

```cpp
/* antes: opt.fmt = FM_FAT | FM_FAT32;  → FatFS elige FAT16 a <32 MiB */
opt.fmt = FM_EXFAT;                       /* ahora: exFAT siempre */
```

Por qué exFAT y no FAT32:
- FAT32 requiere un mínimo de ~32 MiB (65525 clústeres); a 15 MiB es
  **imposible**.
- ExFAT admite volúmenes pequeños (**mínimo 4096 sectores = 2 MiB** en FatFS)
  y es el formato que **Android monta sin preguntar**.
- Con `opt.fmt = FM_EXFAT` solo, FatFS fuerza exFAT a cualquier tamaño
  (`ff.c`, selección de tipo en `f_mkfs`: `(fsopt & FM_ANY) == FM_EXFAT`).

### Cómo quedó el arranque

```
boot
 └─ flash_geom_init()  → JEDEC 0x184068 → 16 MiB → disco 15 MiB
 └─ fatfs_mount()
     ├─ ya hay FS (exFAT/FAT hecho por Android/PC) → MONTA, sin tocar
     └─ región en blanco → f_mkfs(FM_EXFAT) una sola vez → MONTA
 └─ config.txt (VOLUME_LABEL="MiPendrive", etc.) + etiqueta
 └─ se anuncia por USB-MSC con 30720 sectores → Android monta directo
```

### Verificación hecha en la placa real

1. **Vuelco SWD de la MBR** del disco: partición `0x04` FAT16 (estado viejo)
   → después del cambio, `g_fatfs.fs_type == 4` (**FS_EXFAT**) leído en RAM.
2. **fsck/mtools en PC**: el volumen se reconoce como exFAT/FAT válido y
   `config.txt` (255 B) se lee; 15.661.056 B libres.
3. **Prueba final en Android**: la unidad monta directamente sin diálogo de
   formato.

---

## 3. Notas de hardware para reproducir

- Placa: Pico con flash **16 MiB BY25Q128AS** (cualquier Pico RP2040 que
  monte esa flash; el mismo firmware hace 1,5 MiB en la de 2 MiB).
- Programación SWD: scripts `scripts/flash_nosudo.sh` + `debugprobe-openocd.cfg`.
- Para forzar que el firmware reformatee (solo por si se necesita resetear):
  borrar la región de disco con OpenOCD y reiniciar:
  `flash erase_address 0x10100000 0xf00000`.

---

## Referencias

- `docs/README.md` — visión general y funcionalidades.
- `docs/BUILD_INSTRUCTIONS.md` — compilar y grabar.
- `docs/CONFIGURATION_GUIDE.md` — claves de `config.txt`.
- `docs/API_REFERENCE.md` — API de los módulos.
- `../docs/SKILL.md` (raíz) — lecciones aprendidas SWD/OpenOCD.