# Hardware

## Advertencia de 5V

La Pico 2 W **no** es tolerante a 5V. Las salidas de la disquetera (INDEX, RDATA,
TRK0, WPT, DCHG) son open-collector y "tiran" a 5V a traves de pull-ups internos de
la propia disquetera. Hay dos opciones seguras:

1. **Pull-ups a 3.3V**: si la disquetera no lleva pull-up interno (o se desactiva),
   poner resistencias de 1-10 kOhm de cada senal a 3.3V. La Pico solo ve 3.3V como
   nivel alto. Es el metodo mas simple y el habitual en proyectos DIY.
2. **Optoaislador / buffer 3.3V** (recomendado si se quiere aislamiento total).

Las senales de control (DS0, MOTOR, STEP, DIR, WDATA, WG, HS, DENSEL) son entradas
TTL de la disquetera y 3.3V les vale (nivel alto > 2.0V). No hay problema electrico
en ese sentido.

> Comprueba siempre el esquema de tu disquetera antes de conectarla. No conectar
> salidas de 5V directamente a los GPIO de la Pico.

## Mapa de pines

| GPIO | Pin FDD (34p) | Senal | Direccion | Notas |
|------|---------------|-------|-----------|-------|
| 0    | -             | UART TX | out | Depuracion 115200 8N1 |
| 1    | -             | UART RX | in  | |
| 2    | -             | BTN_A | in (pull-up) | Menu: mover / abortar |
| 3    | -             | BTN_B | in (pull-up) | Menu: ejecutar / volver |
| 4    | -             | OLED SDA (i2c0) | in/out | 0x3C, 400 kHz |
| 5    | -             | OLED SCL (i2c0) | in/out | |
| 6    | 1             | DENSEL | out | 1 = alta densidad |
| 7    | 7             | INDEX | in (pull-up) | Pulso activo bajo, por revolucion |
| 8    | 9             | MOTOR | out | Activo bajo (0 = motor on) |
| 9    | 11            | DS0 | out | Activo bajo (0 = drive 0 seleccionada) |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
| 14   | 19            | STEP | out | Baja un paso por dar un 0 |
| 15   | 17            | DIR | out | 1 = hacia fuera (track 0) |
| 16   | 21            | WDATA | out (PIO) | Datos de escritura MFM |
| 17   | 23            | WG | out | Write gate, activo bajo |
| 18   | 25            | TRK0 | in (pull-up) | Activo bajo: cabeza en track 0 |
| 19   | 27            | WPT | in (pull-up) | Activo bajo: disco protegido |
| 20   | 29            | RDATA | in (PIO jmp) | Datos de lectura MFM |
| 21   | 31            | HS | out | 1 = head 0, 0 = head 1 |
| 22   | 33            | DCHG | in (pull-up) | Disk change, activo bajo |
| 26   | 13            | DS1 | out | Sin usar (deseleccionado) |
| 28   | -             | LED | out | LED de estado (opcional) |

Pines 23-25 y 29 no se usan (estan reservados por el conector Wi-Fi de la Pico 2 W).

### Adaptador de la disquetera

Se necesita un conector IDC 26p-34p macho-hembra o cable plano con la pata 1
claramente marcada. El **pin 1** del conector va a DENSEL (GPIO 6). Las senales
del conector de 34 pines (nombres en la tabla) se llevan a los GPIO indicados.

- La disquetera debe estar en **jumper "DS0"** (drive select 0).
- Alimentacion: la disquetera suele necesitar 5V y el motor 12V/5V segun modelo.
  Se alimenta externamente; NO tomar la alimentacion de la Pico.

### Notas por senal

- **DENSEL**: para una disquetera alta densidad de PC, 1 = alta densidad. Al leer
  una imagen DD se pone a 0. En disqueteras de baja densidad puede no hacer nada.
- **HS**: la Pico escribe 1 para cabeza 0; la disquetera lo invierte internamente.
- **INDEX / RDATA / TRK0 / WPT / DCHG**: entradas con pull-up interno de la Pico.
- **RDATA**: se configura como pin `jmp` del PIO (entrada). No conectar tension
  superior a 3.3V.

## MicroSD

SPI1 a 25 MHz en modo "run" (400 kHz en init). Tarjeta **FAT32** (o FAT16) de la
que solo se usa la particion/volumen del sector 0 y se trabaja de forma raw (sin
tabla de particiones MBR). Ver `docs/image-format.md` y `fat.cpp`.

## OLED

SSD1306 128x64 por i2c0 (GPIO 4/5), direccion 0x3C, 400 kHz. Solo
mayusculas ASCII (32-95) por la fuente 5x7 incluida; los textos de la UI estan en
mayusculas a proposito.

## Botones

Activos bajos, con pull-up interno activado en la firmware. BTN_A navega y
tambien aborta la operacion en curso en una lectura/escritura. BTN_B confirma.
