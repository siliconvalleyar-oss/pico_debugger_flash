# Uso

Tras el volcado, la Pico arranca y muestra el menu en el OLED:

```
== FLOPPYCARD ==
  LEER
> ESCRIBIR
  INFO
A:MOVER  B:OK
HD 1.44 / DD 720
```

- **A** mueve la seleccion (LEER / ESCRIBIR / INFO).
- **B** ejecuta la opcion seleccionada.
- Durante una lectura/escritura, **A** aborta la operacion.

## LEER

Lectura de un disquete real y guardado como imagen en la microSD.

1. Pon el disquete en la disquetera.
2. Selecciona LEER y pulsa B.
3. La app detecta el formato (alta densidad 1.44 MB / doble densidad 720 KB)
   a partir de la pista 0 y escribe la imagen en `IMG/DISK0001.IMA`. El numero se
   incrementa (DISK0002, DISK0003, ...) hasta encuentra uno libre.

Pantalla de progreso:

```
LEYENDO    12/160
[====........]    <- barra de avance
A: ABORTAR
```

Resultado:

- `OK  DISK0001`  `SIN ERRORES`  — todas las pistas leidas sin errores.
- `OK  DISK0001`  `ERR EN ALGUNOS` — se grabo la imagen pero con pistas con
  sectores invalidos.
- `ERROR ...` — no hay index (revisa motor/conexion), disco ilegible, SD llena,
  etc.

## ESCRIBIR

Al pulsar B en ESCRIBIR se muestra un **selector de imagen** (navegacion por
slots):

```
ELEGIR IMAGEN
> DISK0001.IMA
  1/5
A:MAS  B:OK
```

- **A** avanza por todas las imagenes de `IMG/` (`.IMA` y `.HFE`), ordenadas
  por nombre.
- **B** graba la imagen mostrada en el disquete real.

Formato para `.ima`: el tamano de la imagen decide (1474560 B -> HD,
737280 B -> DD). Para `.hfe`: se deduce del bitrate del fichero (>=400 kbit/s
-> HD), salvo que `FF.CFG` fuerce `density=hd|dd`.

La disquetera se pone a la densidad correcta y se graba pista a pista,
sincronizada con INDEX. En las `.hfe` se escribe el bitstream crudo por
cilindro/cara (soporte HxC v1/v2/v3, opcodes v3 incluidos).

Errores habituales:

- `DISCO PROTEGIDO` — la lengueta de proteccion esta cerrada.
- `TAMANO IMG NO VALIDO` — el `.ima` no es de 1.44 MB ni 720 KB.
- `HFE INVALIDO` — el `.hfe` no se reconoce (cabecera mala).
- `NO HAY IMGS EN SD` — la carpeta `IMG` esta vacia.

## Configuracion (FF.CFG)

Opcional: un fichero `FF.CFG` en la **raiz** de la microSD ajusta el firmware.
Lineas `clave=valor`, comentarios con `#` o `;`, sin importar mayusculas.

```
# floppydisk
gap3    = 108    # hueco tras cada sector (512 B). 108 por defecto
density = auto   # auto | hd | dd (formato al escribir *.hfe)
```

- `gap3`: bytes de separacion tras cada sector grabado (8.3, solo 512 B).
  Bajarlo aprieta la pista; subirlo la abre. Valores validos 60-190.
- `density`: si es `hd` o `dd`, fuerza ese formato/bitrate al escribir `.hfe`
  (a `auto` se deduce del propio fichero). No afecta a `.ima` ni a LEER.

## INFO

```
SD OK
LIBRE   12345KB
CAP       15MB
```

Muestra el estado de la microSD. Si no hay tarjeta al arrancar, el mensaje
`SIN MICROSD` se muestra al pulsar A y se reintenta.

## MicroSD

La microSD debe estar formateada como FAT16 o FAT32. En el arranque la app crea
la carpeta **`IMG`** si no existe. Las imagenes se guardan con nombre **8.3**
mayusculas (`DISK0001.IMA`). Ver `docs/image-format.md`.