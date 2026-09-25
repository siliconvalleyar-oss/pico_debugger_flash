# Firmware

Este documento describe la arquitectura interna del firmware.

## Flujo global

```
app.cpp  (UI / maquina de estados)
   |--> floppy.cpp   (control de la disquetera + flux PIO)
   |        |--> floppy.pio  (fluxread / fluxwrite)
   |--> mfm.cpp      (CoDec MFM sobre la trama de flux)
   |--> fat.cpp + sd_spi.cpp  (microSD FAT16/FAT32)
   |--> ssd1306.cpp  (OLED)
```

- **LEER** : `floppy_capture_track()` -> `mfm_decode_track()` -> `fat_write_block()`.
- **ESCRIBIR (.ima)**: `fat_read_block()` -> `mfm_encode_track()` -> `floppy_write_track()`.
- **ESCRIBIR (.hfe)**: `fat_read_block()` (datos por cilindro/cara) -> `hfe_flux_for_side()`
  -> `floppy_write_track()`.

## HFE (hfe.cpp) y FF.CFG (cfg.cpp)

- **hfe.cpp** lee imagenes HxC `HXCPICFE` (v1/v2) y `HXCHFEV3` en el mismo
  esquema de fichero que FlashFloppy (cabecera de 512 B, TLUT de 4 B/cilindro,
  datos `[side0 256 B][side1 256 B]` por bloque). `hfe_flux_for_side()` convierte
  el bitstream NRZ (LSB primero) a pulsos de flux en unidades de 24 MHz:
  la celda vale `24000/bitrate` cuentas (MFM), i.e. 48 (DD) o 24 (HD). Los
  opcodes de v3 (Nop/Index/Bitrate/SkipBits/Rand) se procesan igual que en
  `hfe_rdata_flux()` de FlashFloppy. Solo se soporta la **escritura** de HFE;
  leer un disquete lo produce como `.ima`.
- **cfg.cpp** parsea un `FF.CFG` de la raiz y empuja `gap3` al codificador
  (`mfm_set_gap3_512`, por defecto 108) y `density` a la seleccion de formato HFE.
  La lectura del fichero usa `fat_open_root()`.

## PIO (floppy.pio)

`config.h` define `MFM_SAMPLE_FREQ 24e6`: cada unidad de divergencia de flujo es
1/24e6 s (41.67 ns).

### fluxread (captura)

Reloj de la SM = `clk_sys / (3 * 24e6)`. El bucle son exactamente 3 ciclos PIO
por iteracion, por lo que la tasa de conteo es 24 MHz. Mide los tiempos entre
transiciones de RDATA (senal activa a nivel bajo, con la propia transicion como
flanco) y la senal INDEX por el puerto `in`.

El programa emitido (verificado contra Adafruit_Floppy, `arch_rp2.cpp`, con
`static_assert` en `floppy.cpp`):

```
0: jmp x--, 1      ; "wait_one": X-- incondicional (3 ciclos)
1: jmp pin, 3      ; RDATA alto -> pulso
2: jmp 0           ; sigue en low
3: jmp x--, 4      ; "wait_zero" (3 ciclos)
4: jmp pin, 3 [1]  ; espera pulso
5: in pins, 1      ; bit de INDEX (MSB de la mitad de 16 bits)
6: in x, 15        ; cuenta invertida en 15 bits
7: jmp x--, 0      ; decremento de cierre, tiempo contante
```

El ISR acumula dos mitades de 16 bits y hace autopush de 32 bits
(FIFO_JOIN_RX). `read_fifo()` saca primero la mitad baja (la mas antigua) y
guardad en el cache de `half` la alta. El flujo calcula
`delta = (last - data) mod 65536`, `delta /= 2`, y lo recorta a 255
(equivalente a la App de Adafruit). El primer flanco de bajada de INDEX marca
`falling_index_offset`; con `capture_counts != 0` la captura sigue hasta
acumular `capture_ms * 24000` cuentas (por defecto 220 ms).

### fluxwrite (escritura)

Reloj de la SM = `clk_sys / 24e6` (una cuenta en X = 1/24e6 s). Programa:

```
0: set pins, 0      ; WDATA low (tiempo de encendido fijo ~15 ciclos)
1: out x, 16        ; lee el siguiente valor de timing (bloquea si el FIFO esta vacio)
2: nop [14]         ; 15 ciclos en low
3: set pins, 1      ; WDATA high
4: jmp x--, 4       ; mantiene high X cuentas
5: jmp 0            ; siguiente pulso
```

En `write_foreground()` se descuenta `OVERHEAD = 20` (minimo 1) de cada valor antes
de meterlo al FIFO, igual que hace Adafruit. Con `use_index = true` la escritura
espera un flanco de bajada de INDEX tras un flanco de subida (no comienza durante
el pulso), y termina en la siguiente bajada (una revolucion). WG se activa justo
despues de sincronizarse y se desactiva al terminar.

## CoDec MFM (mfm.cpp)

Port fiel de `mfm_impl.h` de Adafruit_Floppy (MIT). Opera sobre los pulsos
capturados `uint8_t[]`.

- **Timings**: `mfm_timings(t, nominal_bit_time_us, 24e6)`:
  - HD (1.0 us): `t1_nom=24`, `t2_max=60`, `t3_max=84`.
  - DD (2.0 us): `t1_nom=48`, `t2_max=120`, `t3_max=168`.
  Los limites son 2.5x y 3.5x del nominal (contado en unidades de 24 MHz).
- **Decode**: escanea la trama en busca de la sincronizacion `0x4489` (A1), usa
  k-means de 10/100/1000 ciclos para clasificar los simbolos, verifica CRC-16
  CCITT y rellena los sectores en orden fisico. Devuelve el numero de sectores
  validos y marca `validity[]`. `logical_track` recibe el cilindro del ultimo
  IDAM.
- **Encode**: genera los gaps estandar de IBM (gap1 50x 0x4E, gap2 22, gap3 108
  por defecto, gap4a 80, presync 12), sincronizaciones A1 `0x4489` y IAM `0x5224`,
  y rellena hasta `max_pulses` con bytes de gap tras el ultimo sector.

Incluye el CoDec portado de Adafruit. En `mfm_io_settings_t.standard_mfm`,
`gap3` (hueco tras cada sector, indexado por `n` de tamano de sector) vale 108
para sectores de 512 B (compatible FlashFloppy; controlable via `mfm_set_gap3_512`/
`FF.CFG`), frente a los 84 de la especificacion IBM antigua.

La deteccion automatica de densidad en `detect_format()` (app.cpp) decodifica la
pista 0 cabeza 0 con umbrales HD y DD; si uno llega al numero completo de
sectores, ese es el formato.

## FAT16/FAT32 minimo (fat.cpp)

Sin tabla de particiones: se usa el BPB del sector 0 (512 B/sector). Soporta
FAT16 y FAT32 (discriminados por `FATSz16==0` + `FATSz32!=0`). La creacion de
imagenes pre-reserva toda la cadena de clusters en RAM (`g_chain[FAT_MAX_CHAIN]`,
fijado por `FAT_MAX_CHAIN` en config.h) y, al finalizar (`fat_finalize`), repinta
la tabla de FAT y la entrada de directorio con el tamano real. Los bloques en
nuevos clusters de directorio se ponen a cero antes de escribir "." y "..".

Operaciones:

- `fat_mount`: parsea el BPB.
- `fat_ensure_img_dir`: crea `IMG` (FAT16: entrada en la raiz; FAT32: primer
  cluster del directorio, con "." y "..").
- `fat_scan_img`: lista `*.IMA` y `*.HFE`, ordenadas por (nombre, extension).
- `fat_create_img` / `fat_open_img` / `fat_open_root` (ficheros 8.3 de la raiz:
  FF.CFG, HFE) / `fat_read_block` / `fat_write_block` / `fat_finalize`.

Ademas, `blk_read`/`blk_write` implementan un cache LRU de 8 bloques de 512 B
por detras de todos los accesos a sector (write-through, asi nunca hay datos
obsoletos). Acelera los barridos de directorio y las lecturas repetidas; la
tabla FAT tiene su propio cache de un sector con *writeback* diferido.

## Concurrencia

`save_and_disable_interrupts()`/`restore_interrupts()` se usan alrededor de la
configuracion del FIFO y el arranque de la SM para que la captura/escritura de
flux no se vea interrumpida (equivalente a `__disable_irq` de Adafruit, usando la
API de la SDK para que funcione igual en ARM y RISC-V).

## Buffers

`TRACK_BUF_SIZE` (256 KiB en RP2350, 128 KiB en RP2040) es un buffer compartido:
trama capturada, trama codificada y, para HFE, los pulsos de flux generados por
`hfe_flux_for_side()`. El buffer de pista de sectores es de 18x512 = 9 KiB. La
Pico 2 W tiene 520 KiB de SRAM; el firmware usa una fraccion modesta.