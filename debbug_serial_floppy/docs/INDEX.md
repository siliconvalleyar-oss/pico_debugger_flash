# Documentación debbug_serial_floppy

## Índice

| Documento | Descripción |
|-----------|-------------|
| [README.md](README.md) | Documentación principal del debugger serie |
| [SKILLS.md](SKILLS.md) | Documentación de todos los skills del repositorio |

## Acceso Rápido

- **Uso básico:** `./bin/pico_serial_debug /dev/ttyACM1 version`
- **Modo interactivo:** `./bin/pico_serial_debug /dev/ttyACM1`
- **Compilar:** `cd debbug_serial_floppy && make`
- **Skills del repo:** Ver [SKILLS.md](SKILLS.md)

## Estructura

```
debbug_serial_floppy/
├── Makefile
├── bin/pico_serial_debug    # Ejecutable (gitignored)
├── obj/                     # Objetos .o (gitignored)
├── include/serial.h         # API SerialPort
├── src/
│   ├── main.cpp             # CLI
│   └── serial.cpp           # termios impl
├── scripts/serial_debug.sh  # Bash wrapper
└── docs/
    ├── README.md            # Este proyecto
    ├── SKILLS.md            # Skills del repo
    └── INDEX.md             # Este archivo
```