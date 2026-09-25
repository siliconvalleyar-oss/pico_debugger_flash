# Formato de imagen

## .ima (raw)

Las imagenes `IMG/DISKxxxx.IMA` son copias raw del disquete, sin cabecera:

- **1.44 MB** (alta densidad): 80 cilindros x 2 cabezas x 18 sectores x 512 B
  = 1474560 bytes.
- **720 KB** (doble densidad): 80 cilindros x 2 cabezas x 9 sectores x 512 B
  = 737280 bytes.

## Orden de los sectores

Indice de bloque (512 B): `(cilindro * 2 + cabeza) * sectores_por_pista + sector`

- Sector 0 = cilindro 0, cabeza 0 (arranque/BPB).
- La cabeza alterna primero dentro del cilindro (cylin 0 head 0, cylin 0 head 1,
  cylin 1 head 0, ...) y el cilindro es el nivel exterior.

Este es el orden estandar de las imagenes FAT del DOS/Windows (BIOS read/write).

## Compatibilidad

- Fat12 de 1.44 MB: compatible (imagen raw sin cambios). Se incluye una imagen de
  prueba autentica en `docs/IBM_PC_FAT12_1_44MB/`.
- Co                                                      `.img`
  o `.ima`; el firmware las trata como raw en ambos sentidos.
- DD de 720 KB (2DD) suele formatearse como FAT12 con 9 sectores por pista.

## Notas del formato de pista (MFM)

El firmware escribe/lee pistas estandar IBM de 512 B con IDAM/CRC-16; consulte
`docs/firmware.md` para gap sizes y la codificacion. La escritura sobrescribe a
partir del pulso de index (una revolucion), por lo que un disquete ya formateado
mantiene los gaps correctos si se sobrescribe completa.

## .hfe (HxC raw)

Ademas de `.ima`, ESCRIBIR acepta imagenes `.hfe` de HxC (v1/v2/v3). Cada
cilindro guarda el bitstream NRZ de ambas caras; el firmware lo convierte a
pulsos de flux y lo graba pista a pista. El formato de fichero (cabecera 512 B,
TLUT de 4 B/cilindro, datos `[cara0 256 B][cara1 256 B]` por bloque) coincide
con FlashFloppy y con las herramientas HxC. Ver `docs/firmware.md`.
