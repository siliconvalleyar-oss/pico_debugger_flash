#!/usr/bin/env python3
"""Genera el header C++ con las fuentes empaquetadas para el driver SSD1963.

Formato de salida (el mismo que espera tft/tft_fonts.cpp):

    data[]    bytes de la fuente, glifo a glifo, sin huecos
    index[]   offsets (uint16_t) dentro de data[], nchars + 1 entradas
    desc      {vert, horiz, nchars, first_char}

    un glifo ocupa vert filas de horiz bits, MSB primero, y los bytes de un
    glifo son (vert * horiz + 7) / 8, de forma que

        rows = vert
        cols = (index[rel + 1] - index[rel]) * 8 / rows

siempre da el ancho real del glifo.

Fuentes admitidas:

  --psf  FICHERO      fuente de consola Linux (PSF1, .psf o .psf.gz).
                      Es la via que usa este repositorio: /usr/share/consolefonts
                      tiene Lat15-VGA8 (8x8) y Lat15-VGA16 (8x16), que son
                      mapas de bits sin antialias, justo lo que necesita un
                      panel TFT.

  --glcd FICHERO      fuente de GLCD Font Creator, con el formato descrito en
                      .kilo/skill/ssd1963-driver/SKILL.md seccion 2:
                      "primer uint16_t = ancho, resto = pares (mascara, datos)".
                      Se deshace el bit-reversal y se reempaqueta por filas
                      MSB-first.

Ejemplos:

    python3 tools/gen_fonts.py --psf /usr/share/consolefonts/Lat15-VGA16.psf.gz \
        --name Font8x16 --out tft/font_glcd_8x16.h
    python3 tools/gen_fonts.py --psf /usr/share/consolefonts/Lat15-VGA8.psf.gz \
        --name Font8x8 --out tft/font_glcd_8x8.h
"""

import argparse
import gzip
import re
import sys

GLYPH_START = 0x20  # desde el espacio


# ---------------------------------------------------------------------------
# fuentes de consola PSF1
# ---------------------------------------------------------------------------
def load_psf(path):
    """Devuelve (filas[code][y][x], width, height, nchars) de una PSF1."""
    if path.endswith(".gz"):
        with gzip.open(path, "rb") as fh:
            raw = fh.read()
    else:
        with open(path, "rb") as fh:
            raw = fh.read()

    if len(raw) < 4 or raw[0] != 0x36 or raw[1] != 0x04:
        raise SystemExit("%s: no es una fuente PSF1" % path)

    mode = raw[2]
    charsize = raw[3]
    # PSF1: mode bit 0 = tabla Unicode detras de los glifos, bit 1 = 512
    # glifos.  Da igual: lo unico que necesita el driver es el rango ASCII
    # 0x20..0x7F, que es compatible en todas estas fuentes.
    # PSF1: 8 pixeles de ancho y un byte por fila, asi que la altura es
    # directamente el tamano del glifo (8 para VGA8, 16 para VGA16).  El bit 0
    # del modo solo indica si hay tabla Unicode detras, y da igual: lo unico
    # que necesita el driver es el rango ASCII 0x20..0x7F, que es compatible
    # en todas estas fuentes.
    width = 8
    height = charsize
    if charsize not in (8, 16):
        raise SystemExit("%s: glifos de %d bytes, solo se admiten 8 o 16"
                         % (path, charsize))

    body = raw[4:]
    nchars = len(body) // charsize
    if nchars < 256:
        raise SystemExit("%s: solo %d glifos, hacen falta 256 para ASCII"
                         % (path, nchars))

    glyphs = []
    for code in range(nchars):
        g = body[code * charsize:(code + 1) * charsize]
        glyphs.append([[(g[y] >> (7 - x)) & 1 for x in range(width)]
                       for y in range(height)])
    return glyphs, width, height, nchars


# ---------------------------------------------------------------------------
# fuentes de GLCD Font Creator
# ---------------------------------------------------------------------------
def load_glcd(path, first_char, nchars):
    """Parsea un .c de GLCD Font Creator con el layout de la seccion 2 del skill.

    Cada glifo es  {uint16_t ancho, (uint16_t mascara, uint16_t datos) * k}
    con los bits de datos en orden columna a columna y LSB primero.  Se
    deshace el bit-reversal y se reempaqueta por filas, MSB primero.
    """
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()

    m = re.search(r"FontSize\s*:\s*(\d+)\s*[xX]\s*(\d+)", text)
    if not m:
        raise SystemExit("%s: no se encuentra '//GLCD FontSize : W x H'" % path)
    declared_w, height = int(m.group(1)), int(m.group(2))

    values = [int(tok, 0) for tok in
              re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", text.split("FontSize")[1])]
    values = [v for v in values if v <= 0xFFFF]

    glyphs = []
    i = 0
    while i < len(values) and len(glyphs) < nchars:
        width = values[i] & 0xFF
        i += 1
        if width == 0 or width > declared_w + 1:
            i += 1
            continue
        bits = [[0] * width for _ in range(height)]
        col = 0
        while col < width and i + 1 < len(values):
            mask, data = values[i], values[i + 1]
            i += 2
            for b in range(16):
                if mask & (1 << b):
                    col = b
                    break
            for b in range(16):
                if data & (1 << b):
                    bits[b % height][col] = 1
            col += 1
        glyphs.append(bits)

    if not glyphs:
        raise SystemExit("%s: no se han encontrado glifos" % path)
    return glyphs, width, height, len(glyphs)


# ---------------------------------------------------------------------------
# empaquetado
# ---------------------------------------------------------------------------
def pack(glyphs, width, height, first_char, nchars, name):
    stride = (width + 7) // 8
    data = bytearray()
    index = [0]
    for code in range(first_char, first_char + nchars):
        rel = code - first_char
        glyph = glyphs[rel] if rel < len(glyphs) else \
            [[0] * width for _ in range(height)]
        for y in range(height):
            for b in range(stride):
                byte = 0
                for x in range(b * 8, min((b + 1) * 8, width)):
                    if glyph[y][x]:
                        byte |= 1 << (7 - (x - b * 8))
                data.append(byte)
        index.append(len(data))

    out = []
    out.append("// Generado por tools/gen_fonts.py -- no editar a mano.\n")
    out.append("#ifndef TFT_FONT_%s_H\n#define TFT_FONT_%s_H\n\n" % (name, name))
    out.append('#include "tft_fonts.h"\n\n')
    out.append("namespace tft {\n\n")
    out.append("static const uint8_t %s_data[%d] = {\n" % (name, len(data)))
    for i in range(0, len(data), 12):
        row = ", ".join("0x%02x" % b for b in data[i:i + 12])
        out.append("    %s,\n" % row)
    out.append("};\n\n")
    out.append("static const uint16_t %s_index[%d] = {\n" % (name, len(index)))
    for i in range(0, len(index), 12):
        row = ", ".join("%d" % v for v in index[i:i + 12])
        out.append("    %s,\n" % row)
    out.append("};\n\n")
    out.append("static const TftFont %s = {\n" % name)
    out.append("    %s_data,\n" % name)
    out.append("    %s_index,\n" % name)
    out.append("    %d,   // vert  (filas)\n" % height)
    out.append("    %d,   // horiz (columnas)\n" % width)
    out.append("    %d,   // nchars\n" % nchars)
    out.append("    %d,   // first_char\n" % first_char)
    out.append("};\n\n")
    out.append("}  // namespace tft\n\n")
    out.append("#endif  // TFT_FONT_%s_H\n" % name)
    return "".join(out)


def preview(glyphs, width, height, text):
    """Dibuja texto en la consola: sirve para comprobar la fuente a ojo."""
    rows = [""] * height
    for ch in text:
        idx = ord(ch) - GLYPH_START
        glyph = glyphs[idx] if 0 <= idx < len(glyphs) else None
        for y in range(height):
            if glyph:
                for x in range(width):
                    rows[y] += "##" if glyph[y][x] else "  "
            else:
                rows[y] += "??" * width
    print("+" + "-" * (len(rows[0])) + "+")
    for r in rows:
        print("|" + r + "|")
    print("+" + "-" * (len(rows[0])) + "+")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--psf", help="fuente de consola PSF1")
    ap.add_argument("--glcd", help="fuente de GLCD Font Creator (.c)")
    ap.add_argument("--name", default="Font8x16", help="nombre del simbolo C++")
    ap.add_argument("--out", help="header de salida")
    ap.add_argument("--first-char", type=int, default=GLYPH_START)
    ap.add_argument("--chars", type=int, default=96,
                    help="numero de glifos a empaquetar (ASCII imprimible)")
    ap.add_argument("--preview", metavar="TEXTO",
                    help="dibuja texto con la fuente ya empaquetada y sale")
    args = ap.parse_args()

    if args.psf:
        glyphs, width, height, nchars = load_psf(args.psf)
        first = args.first_char
        count = min(args.chars, nchars - args.first_char)
    elif args.glcd:
        glyphs, width, height, nchars = load_glcd(args.glcd, args.first_char,
                                                  args.chars)
        first, count = args.first_char, len(glyphs)
    else:
        raise SystemExit("falta --psf o --glcd")

    if args.preview:
        preview(glyphs, width, height, args.preview)
        if not args.out:
            return
    if args.out:
        with open(args.out, "w", encoding="utf-8") as fh:
            fh.write(pack(glyphs, width, height, first, count, args.name))
        print("%s: %dx%d, %d glifos desde %d (0x%02X), %d bytes de datos"
              % (args.out, width, height, count, first, first,
                 count * height * ((width + 7) // 8)))


if __name__ == "__main__":
    sys.exit(main())
