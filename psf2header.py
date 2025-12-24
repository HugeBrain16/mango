import sys
import struct

args = sys.argv[1:]
file = None

psf_version = None
psf_glyph_size = None
psf_length = 0
psf_height = None
psf_width = None

psf1_mode = None

psf2_version = None
psf2_header_size = None

c_header = ""

if len(args) > 0:
    file = args[0]
else:
    sys.exit("usage: psf2header <.psf file>")

if not file.endswith(".psf"):
    sys.exit("not a .psf file")

with open(file, 'rb') as psf:
    header = psf.read(4)

    if header[0] == 0x72 and header[1] == 0xb5 and header[2] == 0x4a and header[3] == 0x86:
        psf_version = 2
    elif header[0] == 0x36 and header[1] == 0x04:
        psf_version = 1
        psf.seek(2)

    if psf_version is None:
        sys.exit("not a valid .psf file")

    if psf_version == 2:
        psf2_version = struct.unpack("<I", psf.read(4))[0]
        psf2_header_size = struct.unpack("<I", psf.read(4))[0]

        psf_has_unicode_table = bool(struct.unpack("<I", psf.read(4))[0] & 1<<1) 
        psf_length = struct.unpack("<I", psf.read(4))[0]
        psf_glyph_size = struct.unpack("<I", psf.read(4))[0]
        psf_height = struct.unpack("<I", psf.read(4))[0]
        psf_width = struct.unpack("<I", psf.read(4))[0]

        psf.seek(psf2_header_size)
    elif psf_version == 1:
        psf_width = 8
        psf_height = 8

        psf1_mode = psf.read(1)
        if psf1_mode == 0x01:
            psf_length = 512
        elif psf1_mode in (0x02, 0x04):
            psf_has_unicode_table = True
        else:
            psf_length = 256

        psf_glyph_size = struct.unpack("<B", psf.read(1))[0]
        psf.seek(4)

    c_header += f"""#include <stdint.h>

#define FONT_WIDTH {psf_width}
#define FONT_HEIGHT {psf_height}
#define FONT_GLYPHS {psf_length}
#define FONT_GLYPH_SIZE {psf_glyph_size}

static const uint8_t font_bitmaps[FONT_GLYPHS][FONT_GLYPH_SIZE] = {{
"""

    for i in range(psf_length):
        c_header += "\t{" + ", ".join(f"0x{b:02x}" for b in psf.read(psf_glyph_size))

        if (i == psf_length - 1):
            c_header += "}\n"
        else:
            c_header += "},\n"
    c_header += "};"

    print(c_header)
