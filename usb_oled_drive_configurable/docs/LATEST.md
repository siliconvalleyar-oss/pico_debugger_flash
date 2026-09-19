# Últimos cambios, diagnóstico y recuperación del pendrive

Documento **vivo** que registra los cambios de firmware más recientes del
pendrive, el incidente de corrupción del directorio raíz (exFAT) y su
restauración, con todos los datos técnicos necesarios para reproducir el
diagnóstico. Complementa a `ANDROID_VALIDATION.md`, `CONFIGURATION_GUIDE.md`
y `TROUBLESHOOTING.md`.

Fecha de la última validación: **2026-09-19**.

---

## 1. Cambio 1 — Caché de escritura lenta (write-behind) de 4 KiB

### Problema que resuelve

Antes, cada sector lógico de 512 B que el host enviaba por USB (SCSI WRITE10)
disparaba un **borrado + programación completa de 4 KiB de flash** con las
interrupciones desactivadas:

```
por sector:
  save_and_disable_interrupts()
  flash_range_erase(bloque, 4096)   // ~40 ms con IRQ disabled
  flash_range_program(bloque, buf, 4096)
  restore_interrupts()
```

Un borrado de 4 KiB tarda ~40 ms con el USB bloqueado. Al copiar una foto de
**~5 MB** (≈ 10 000 sectores), el host superaba su timeout y **truncaba la
copia** (el archivo quedaba en ~376 KB).

### Solución implementada

`lib/fatfs/source/diskio.c` ahora usa una **caché write-behind de un bloque
de 4 KiB en RAM** (variables estáticas):

```c
static uint8_t  block_cache[FLASH_ERASE_SIZE];   // 4096 B en RAM
static uint32_t cache_base = 0xFFFFFFFFu;        // offset flash del bloque en caché
static bool     cache_dirty = false;             // caché más nueva que flash
static bool     cache_blank = false;             // bloque en flash = todo 0xFF
```

Flujo:

- `disk_write()` acumula los sectores en `block_cache` y marca `cache_dirty`.
- El bloque solo se **commitea** (erase+program) cuando:
  1. la escritura salta a otro bloque de 4 KiB, o
  2. llega `CTRL_SYNC` (`disk_ioctl`), o
  3. llega `SYNCHRONIZE_CACHE_10` del host → `disk_flush()`.
- Si el bloque en flash **ya está en blanco** (0xFF) se omite el borrado y solo
  se programa (más rápido). El borrado + programación siempre van juntos y con
  las IRQ desactivadas, pero solo **una vez por bloque** en vez de por sector.

### API nueva

`lib/fatfs/source/diskio.h`:

```c
DRESULT disk_flush (void);   // fuerza la caché a flash
```

Cableado desde el SCSI (`src/usb_storage.cpp`,
`tud_msc_synchronize_cache_cb`):

```c
fatfs_sync();   // vacía buffers internos de FatFS
disk_flush();   // vacía la caché write-behind de la capa de disco
```

De esta forma, cuando el host hace `sync`/desmontaje antes de desconectar, los
datos llegan a flash de forma duradera.

### Estado

La **caché está flasheada en el firmware actual**. Los símbolos se confirmaron
en el ELF:

| Símbolo | Dirección |
|---|---|
| `block_cache` | `0x20003DDC` |
| `cache_base` | `0x2000116C` |
| `cache_blank` | `0x2000537C` |
| `cache_dirty` | `0x2000537D` |
| `disk_read` | `0x1000CF75` |
| `disk_write` | `0x1000D059` |

> **Pendiente (abierto):** la copia grande sigue siendo sensible. En la
> Raspberry, copiar 5 MB puede desconectar el dispositivo del bus USB
> (`device offline`, re-enumeración fallida con `error -22/-71`). Es el mismo
> problema de fondo del truncado original (ventana larga con IRQ off durante
> el erase+program dentro del callback SCSI) y aún no está cerrado.

---

## 2. Cambio 2 — Incidente: directorio raíz exFAT borrado y recuperación

> ⚠️ **Importante para la configuración:** si el pendrive vuelve a pedir
> "formatear" en Android o demuestra `failed to load alloc-bitmap` en Linux,
> es casi seguro que este directorio raíz se borró. La recuperación se describe
> abajo.

### Síntoma

- Al conectar el pendrive a Android: **"pide ser formateada"**.
- En la Raspberry (`dmesg`):

```
exFAT-fs (sda1): failed to load alloc-bitmap
exFAT-fs (sda1): failed to recognize exfat type
device offline error, dev sda, sector ... op 0x1:(WRITE)
Buffer I/O error on dev sda1, logical block 89, lost async page write
```

- `lsblk` sí ve la partición `sda1` con FSTYPE exFAT (MBR y boot sector OK), pero
  no la monta.

### Diagnóstico (con OpenOCD / SWD)

Se comparó el **estado vivo** de la flash contra la imagen validada
(`docs` de trabajo, fsck.exfat = clean, 25 dir / 4 archivos). Resultado del
diff por bloques de 4 KiB en los primeros 128 KB:

```
block  0 (0x0000): 3584 B diff  → sector 0 (MBR) OK, resto gap 0x00
block  1 (0x1000): todo diff     → gap entre MBR y partición
block  2 (0x2000): todo diff     → gap
block  3 (0x3000): todo diff     → gap
block  4 (0x4000): 1024 B diff   → gap
block 19 (0x13000): 4094 B diff  → ⭐ CLUSTER 5 (root dir) borrado a 0xFF
```

**Única corrupción significativa:** el clúster 5 = directorio raíz exFAT,
borrado a `0xFF`. Boot sector, FAT, MBR y resto de datos **intactos**.

Por qué rompe el montaje: en exFAT, el **alloc-bitmap** (entrada de tipo
`0x81`) vive en el directorio raíz. Sin root dir, el kernel no encuentra el
bitmap → `failed to load alloc-bitmap` → Android ofrece formatear.

Arrancar el firmware escribió sobre ese clúster (`config.txt` + etiqueta
"MiPendrive" son entradas del root dir); la hipótesis más probable es un
**corte de energía / desconexión USB entre el erase y el program** de ese
bloque.

### Datos técnicos del volumen (boot sector exFAT)

Ubicación del boot sector exFAT: **sector de partición 0 = disco sector 63 =
flash offset `0x107E00`** (`file`: "DOS/MBR boot sector; partition 1 start 63").

| Campo | Valor |
|---|---|
| FS name (`OEM/FSName`) | `EXFAT   ` |
| `PartitionOffset` | 63 |
| `VolumeLength` | 30657 |
| `FatOffset` | 32 |
| `FatLength` | 30 |
| `ClusterHeapOffset` | 65 |
| `ClusterCount` | 3824 |
| `RootDirCluster` | **5** |
| `VolumeSerialNumber` | `0x542177C1` |
| Sectores por clúster (`csize`) | 8 (= 4096 B/clúster) |

### Geometría útil (para reproducir)

```
clúster N (partición)        = ClusterHeapOffset(65) + (N-2)*8
clúster 5 (root dir)         = sector partición 89
                             = disco sector 63 + 89 = 152
                             = flash offset 0x100000 + 152*512 = 0x113000
                             = dirección XIP 0x10113000
FAT                         = disco sectores 95..124 (FatOffset 32 + FatLength 30)
```

### Restauración (root dir roto)

Se extrajo el bloque de 4096 B de la imagen validada
(`disk_full.img[0x13000:0x14000]` → `root_clus5.bin`) y se escribió con
OpenOCD:

```bash
openocd -c "init; reset halt; \
  flash write_image /tmp/opencode/root_clus5.bin 0x10113000 bin; reset run"
```

Verificación posterior (byte a byte, cuenta de md5):

```
flash offset 0x10113000 == root_clus5.bin   ✅ (md5 230d7853...a042aba5)
primeros bytes: 83 0a 4d 00 69 00 50 ...     → entrada 0x83 label "MiPendrive"
```

### Verificación final del estado montado

Lectura del objeto global del sistema de archivos tras reiniciar el firmware:

```
FATFS (g_fatfs @ 0x200023C8):
  fs_type   = 0x04 (FS_EXFAT)   ✅ montado
  n_fatent  = 0x0EF2
  fsize     = 0x1E  (30 sectores FAT)
  volbase   = 0x3F  (63)
  fatbase   = 0x5F  (95)
  dirbase   = 0x05  (5 = root dir)
  database  = 0x80
  bitbase   = 0x80

STATE (g_state @ 0x20004F38):
  mounted = true, total = 14 MB (0x0E), free = 14 MB (0x0E)
```

En la Raspberry: `lsblk` → `sda1  exfat  MiPendrive  /media/joy/MiPendrive`
(se monta por primera vez con el aviso "Volume was not properly unmounted" solo
por el corte anterior).

---

## 3. Dónde buscar la corrupción (checklist)

| Paso | Comando |
|---|---|
| Dump del disco (128 KB) | `dump_image /tmp/opencode/cur.img 0x10100000 0x20000` |
| Dump del clúster 5 | `dump_image /tmp/opencode/cl5.bin 0x10113000 0x1000` |
| Boot sector exFAT | `dump_image /tmp/opencode/boot.bin 0x10107E00 0x200` |
| Estado FATFS/geom | leer `0x200023C8` (FATFS) / `0x20001160` (g_geom) |
| Comparar con imagen validad | `fsck.exfat` sobre la partición + diff de bloques |

Nota OpenOCD: usar `adapter speed 100` (a 1000 kHz la conexión SWD falla) y
siempre `reset run; sleep 4000; halt` antes de `dump_image`.

---

## 4. Referencia rápida de configuración (recordatorio)

Recordatorio de las claves de `config.txt` (detalle en
`CONFIGURATION_GUIDE.md`):

| Clave | Valores | Default |
|---|---|---|
| `VOLUME_LABEL` | 1..11 caracteres, sin espacios | `MiPendrive` |
| `READ_ONLY` | `0`/`1` | `0` |
| `ENABLE_OLED` | `0`/`1` | `1` |
| `LED_ON_CONNECT` | `0`/`1` | `1` |
| `AUTO_MOUNT_DELAY_MS` | `0`..`60000` | `500` |

La etiqueta y `config.txt` son **entradas del directorio raíz (clúster 5)**;
cualquier corte de energía durante su escritura puede dañar ese clúster, que es
el que hace fallar el montaje exFAT. Preferiblemente expulsa/sincroniza antes de
desconectar.

---

## 5. Archivos modificados en este ciclo

| Archivo | Cambio |
|---|---|
| `lib/fatfs/source/diskio.c` | Caché write-behind 4 KiB + `cache_flush()` |
| `lib/fatfs/source/diskio.h` | Nuevo prototipo `disk_flush()` |
| `src/usb_storage.cpp` | `SYNCHRONIZE_CACHE_10` → `fatfs_sync()` + `disk_flush()` |
| `docs/` | Este documento + actualización de guías (Android, configuración) |
| `docs/CONFIGURATION_GUIDE.md` | Ajustes editoriales |
| `docs/ANDROID_VALIDATION.md` | Ajustes editoriales |