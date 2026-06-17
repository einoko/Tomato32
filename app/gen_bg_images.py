#!/usr/bin/env python3
"""Convert PNG background images to LVGL RGB565 C arrays.

Uses only Python stdlib (zlib + struct) — no third-party packages required.

Usage:
    python3 gen_bg_images.py <output_dir> [name:path ...]

Each name:path pair generates:
    <output_dir>/custom_background_<name>.c + .h

Also generates:
    <output_dir>/bg_config.h  (HAVE_CUSTOM_BG_<NAME> defines for each present image)
"""

import os
import struct
import sys
import zlib

# ---------------------------------------------------------------------------
# Minimal pure-Python PNG decoder (RGB and RGBA, 8-bit, with Adam7 support)
# ---------------------------------------------------------------------------

_ADAM7_PASSES = [
    # (x_start, y_start, x_step, y_step)
    (0, 0, 8, 8),
    (4, 0, 8, 8),
    (0, 4, 4, 8),
    (2, 0, 4, 4),
    (0, 2, 2, 4),
    (1, 0, 2, 2),
    (0, 1, 1, 2),
]


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _defilter(filter_type, scanline, prev, bpp):
    n = len(scanline)
    out = bytearray(n)
    if filter_type == 0:
        out[:] = scanline
    elif filter_type == 1:
        for i in range(n):
            a = out[i - bpp] if i >= bpp else 0
            out[i] = (scanline[i] + a) & 0xFF
    elif filter_type == 2:
        for i in range(n):
            b = prev[i] if prev else 0
            out[i] = (scanline[i] + b) & 0xFF
    elif filter_type == 3:
        for i in range(n):
            a = out[i - bpp] if i >= bpp else 0
            b = prev[i] if prev else 0
            out[i] = (scanline[i] + (a + b) // 2) & 0xFF
    elif filter_type == 4:
        for i in range(n):
            a = out[i - bpp] if i >= bpp else 0
            b = prev[i] if prev else 0
            c = prev[i - bpp] if (prev and i >= bpp) else 0
            out[i] = (scanline[i] + _paeth(a, b, c)) & 0xFF
    else:
        raise ValueError(f"Unknown PNG filter type: {filter_type}")
    return bytes(out)


def _decode_pass(raw, pos, pw, ph, bpp):
    """Decode one interlace pass (or the full image for non-interlaced)."""
    stride = pw * bpp
    rows = []
    prev = None
    for _ in range(ph):
        ftype = raw[pos]
        pos += 1
        row = _defilter(ftype, raw[pos : pos + stride], prev, bpp)
        pos += stride
        rows.append(row)
        prev = row
    return rows, pos


def read_png_rgb(path):
    """Return (width, height, pixels) where pixels is flat RGB888 bytes."""
    with open(path, "rb") as f:
        data = f.read()

    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG file")

    pos = 8
    w = h = bit_depth = color_type = interlace = 0
    idat = bytearray()

    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos : pos + 4])
        ctype = data[pos + 4 : pos + 8]
        cdata = data[pos + 8 : pos + 8 + length]
        pos += 12 + length

        if ctype == b"IHDR":
            w, h = struct.unpack(">II", cdata[:8])
            bit_depth, color_type = cdata[8], cdata[9]
            interlace = cdata[12]
            if bit_depth != 8:
                raise ValueError(f"Only 8-bit PNG supported (got {bit_depth})")
            if color_type not in (2, 6):
                raise ValueError(
                    f"Only RGB/RGBA PNG supported (color_type={color_type})"
                )
        elif ctype == b"IDAT":
            idat.extend(cdata)
        elif ctype == b"IEND":
            break

    channels = 3 if color_type == 2 else 4
    bpp = channels  # 8-bit: 1 byte per channel

    raw = zlib.decompress(bytes(idat))

    if interlace == 0:
        rows, _ = _decode_pass(raw, 0, w, h, bpp)
        pixels = bytearray().join(rows)
    else:
        # Adam7 interlacing
        pixels = bytearray(w * h * channels)
        pos = 0
        for x0, y0, xs, ys in _ADAM7_PASSES:
            pw = (w - x0 + xs - 1) // xs if x0 < w else 0
            ph = (h - y0 + ys - 1) // ys if y0 < h else 0
            if pw == 0 or ph == 0:
                continue
            rows, pos = _decode_pass(raw, pos, pw, ph, bpp)
            for pr, row in enumerate(rows):
                ay = y0 + pr * ys
                for pc in range(pw):
                    ax = x0 + pc * xs
                    src = pc * channels
                    dst = (ay * w + ax) * channels
                    pixels[dst : dst + channels] = row[src : src + channels]

    # Strip alpha channel if RGBA
    if channels == 4:
        out = bytearray(w * h * 3)
        for i in range(w * h):
            out[i * 3 : i * 3 + 3] = pixels[i * 4 : i * 4 + 3]
        pixels = out

    return w, h, bytes(pixels)


# ---------------------------------------------------------------------------
# RGB888 → RGB565 little-endian
# ---------------------------------------------------------------------------


def rgb888_to_rgb565_le(pixels):
    data = bytearray(len(pixels) // 3 * 2)
    idx = 0
    for i in range(0, len(pixels), 3):
        r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data[idx] = v & 0xFF
        data[idx + 1] = (v >> 8) & 0xFF
        idx += 2
    return data


# ---------------------------------------------------------------------------
# C file generation
# ---------------------------------------------------------------------------


def png_to_lvgl_c(png_path, out_dir, var_name):
    w, h, pixels = read_png_rgb(png_path)
    data = rgb888_to_rgb565_le(pixels)
    stride = w * 2

    guard = var_name.upper() + "_H"
    header_path = os.path.join(out_dir, f"{var_name}.h")
    with open(header_path, "w") as f:
        f.write(f"#ifndef {guard}\n")
        f.write(f"#define {guard}\n\n")
        f.write('#include "lvgl.h"\n\n')
        f.write(f"extern const lv_image_dsc_t {var_name};\n\n")
        f.write(f"#endif /* {guard} */\n")

    c_path = os.path.join(out_dir, f"{var_name}.c")
    with open(c_path, "w") as f:
        f.write(f'#include "{var_name}.h"\n\n')
        f.write(
            f"/* {w}x{h} RGB565 -- generated by gen_bg_images.py, do not edit */\n\n"
        )
        f.write("/* clang-format off */\n")
        f.write(f"static const uint8_t {var_name}_data[] = {{\n")
        for i in range(0, len(data), 16):
            chunk = data[i : i + 16]
            f.write("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",\n")
        f.write("};\n")
        f.write("/* clang-format on */\n\n")
        f.write(f"const lv_image_dsc_t {var_name} = {{\n")
        f.write("    .header.magic  = LV_IMAGE_HEADER_MAGIC,\n")
        f.write("    .header.cf     = LV_COLOR_FORMAT_RGB565,\n")
        f.write("    .header.flags  = 0,\n")
        f.write(f"    .header.w      = {w},\n")
        f.write(f"    .header.h      = {h},\n")
        f.write(f"    .header.stride = {stride},\n")
        f.write(f"    .data_size     = {len(data)},\n")
        f.write(f"    .data          = {var_name}_data,\n")
        f.write("};\n")

    print(f"  {var_name}: {w}x{h}, {len(data)} bytes -> {c_path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(f"Usage: {sys.argv[0]} <output_dir> [name:path ...]")

    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)

    names = []
    for arg in sys.argv[2:]:
        if ":" not in arg:
            sys.exit(f"Expected name:path, got: {arg!r}")
        name, path = arg.split(":", 1)
        png_to_lvgl_c(path, out_dir, f"custom_background_{name}")
        names.append(name)

    config_path = os.path.join(out_dir, "bg_config.h")
    with open(config_path, "w") as f:
        f.write("#ifndef BG_CONFIG_H\n#define BG_CONFIG_H\n\n")
        for name in names:
            f.write(f"#define HAVE_CUSTOM_BG_{name.upper()}\n")
        f.write("\n#endif /* BG_CONFIG_H */\n")
    print(f"  bg_config.h: {len(names)} background(s) -> {config_path}")
    print("Done.")
