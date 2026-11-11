#!/usr/bin/env python3
"""Generate deterministic geometric footage for the AE prototype's tests.

These are algorithmic test fixtures, not final particle art or quality targets.
Every frame has a fixed canvas; subject motion exists entirely in image pixels.
Only Python's standard library is required.
"""

from __future__ import annotations

import argparse
import binascii
import colorsys
import json
import math
from pathlib import Path
import struct
import zlib


WIDTH, HEIGHT, FPS, FRAME_COUNT = 640, 360, 25, 100
SIGNATURE = b"\x89PNG\r\n\x1a\n"


def clamp(value: float, lower: float = 0.0, upper: float = 1.0) -> float:
    return max(lower, min(upper, value))


def chunk(kind: bytes, payload: bytes) -> bytes:
    crc = binascii.crc32(kind + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", crc)


def write_png(path: Path, pixels: bytes | bytearray, width: int, height: int) -> None:
    if len(pixels) != width * height * 4:
        raise ValueError("Pixel buffer does not match RGBA dimensions")
    # Filter 0 makes independent standard-library verification uncomplicated.
    rows = bytearray()
    row_bytes = width * 4
    for y in range(height):
        rows.append(0)
        rows.extend(pixels[y * row_bytes:(y + 1) * row_bytes])
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    encoded = (SIGNATURE + chunk(b"IHDR", header)
               + chunk(b"sRGB", b"\x00")
               + chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)


def read_png(path: Path) -> tuple[int, int, bytes]:
    """Validate CRCs, RGBA format and row layout, then return decoded pixels."""
    encoded = path.read_bytes()
    if not encoded.startswith(SIGNATURE):
        raise ValueError(f"Invalid PNG signature: {path}")
    cursor, compressed, dimensions = len(SIGNATURE), bytearray(), None
    saw_end = False
    while cursor < len(encoded):
        if cursor + 12 > len(encoded):
            raise ValueError(f"Truncated PNG chunk: {path}")
        length = struct.unpack_from(">I", encoded, cursor)[0]
        kind = encoded[cursor + 4:cursor + 8]
        end = cursor + 8 + length
        if end + 4 > len(encoded):
            raise ValueError(f"Truncated PNG payload: {path}")
        payload = encoded[cursor + 8:end]
        crc = struct.unpack_from(">I", encoded, end)[0]
        if crc != (binascii.crc32(kind + payload) & 0xFFFFFFFF):
            raise ValueError(f"PNG CRC mismatch: {path}")
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            if (depth, color, compression, filtering, interlace) != (8, 6, 0, 0, 0):
                raise ValueError(f"Unexpected PNG encoding: {path}")
            dimensions = (width, height)
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            saw_end = True
            if end + 4 != len(encoded):
                raise ValueError(f"Unexpected data after PNG end: {path}")
            break
        cursor = end + 4
    if dimensions is None or not saw_end:
        raise ValueError(f"Missing PNG header or end: {path}")
    width, height = dimensions
    raw = zlib.decompress(compressed)
    row_bytes = width * 4
    if len(raw) != height * (row_bytes + 1):
        raise ValueError(f"PNG decompressed size mismatch: {path}")
    decoded = bytearray()
    for y in range(height):
        offset = y * (row_bytes + 1)
        if raw[offset] != 0:
            raise ValueError(f"Unexpected PNG row filter: {path}")
        decoded.extend(raw[offset + 1:offset + row_bytes + 1])
    return width, height, bytes(decoded)


def source_center(frame: int) -> tuple[float, float]:
    phase = frame / (FRAME_COUNT - 1) * math.pi * 1.65 - math.pi * 0.9
    return (WIDTH * (0.5 + 0.31 * math.sin(phase)),
            HEIGHT * (0.5 + 0.24 * math.sin(phase * 2 + 0.55)))


def map_color(x: float, y: float) -> tuple[float, float, float]:
    hue = (0.60 + x / WIDTH * 0.52 + 0.045 * math.sin(y / HEIGHT * math.tau)) % 1.0
    return colorsys.hsv_to_rgb(hue, 0.72, 1.0)


def make_frame(frame: int) -> tuple[bytearray, bytearray]:
    rgba = bytearray(WIDTH * HEIGHT * 4)
    # External luma pass deliberately has opaque black outside its white/gray
    # subject, to exercise luma rather than accidentally relying on alpha.
    luma = bytearray(b"\x00\x00\x00\xff" * (WIDTH * HEIGHT))
    cx, cy = source_center(frame)
    rotation = frame / FPS * 1.3
    reach = 41
    x0, x1 = max(0, math.floor(cx - reach)), min(WIDTH - 1, math.ceil(cx + reach))
    y0, y1 = max(0, math.floor(cy - reach)), min(HEIGHT - 1, math.ceil(cy + reach))
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            radius, angle = math.hypot(dx, dy), math.atan2(dy, dx) - rotation
            outer = 34 + 2.2 * math.sin(angle * 7)
            inner = 22 + 1.4 * math.sin(angle * 5 + 0.7)
            coverage = clamp(outer + 0.5 - radius) * clamp(radius - inner + 0.5)
            if coverage <= 0:
                continue
            # Features rotate with the rasterized subject, providing texture
            # useful for future optical-flow validation, including local motion.
            facet = 0.55 + 0.45 * (0.5 + 0.5 * math.sin(angle * 11 + radius * 0.22))
            highlight = 0.78 + 0.22 * max(0.0, math.cos(angle - 0.8)) ** 7
            brightness = facet * highlight
            red, green, blue = map_color(x + 0.5, y + 0.5)
            index = (y * WIDTH + x) * 4
            rgba[index:index + 4] = bytes((
                round(255 * red * brightness), round(255 * green * brightness),
                round(255 * blue * brightness), round(255 * coverage)))
            value = round(255 * coverage * (0.35 + 0.65 * facet))
            luma[index:index + 4] = bytes((value, value, value, 255))
    return rgba, luma


def alpha_bbox(pixels: bytes) -> tuple[int, int, int, int]:
    positions = [i for i, value in enumerate(pixels[3::4]) if value > 0]
    if not positions:
        raise ValueError("RGBA source has no visible subject")
    xs, ys = [i % WIDTH for i in positions], [i // WIDTH for i in positions]
    return min(xs), min(ys), max(xs), max(ys)


def verify(root: Path) -> dict:
    boxes, alpha_sums = [], []
    sample_indices = {0, 25, 50, 75, 99}
    for frame in range(FRAME_COUNT):
        name = f"frame_{frame:04d}.png"
        w, h, rgba = read_png(root / "rgba" / name)
        lw, lh, luma = read_png(root / "luma" / name)
        if (w, h, lw, lh) != (WIDTH, HEIGHT, WIDTH, HEIGHT):
            raise ValueError(f"Wrong fixture dimensions at frame {frame}")
        if any(alpha != 255 for alpha in luma[3::4]):
            raise ValueError(f"External luma frame must be opaque: {frame}")
        if not any(rgba[3::4]) or not any(a == 0 for a in rgba[3::4]):
            raise ValueError(f"RGBA frame must contain subject and transparency: {frame}")
        if not any(0 < a < 255 for a in rgba[3::4]):
            raise ValueError(f"RGBA frame lacks anti-aliased alpha: {frame}")
        if frame in sample_indices:
            boxes.append({"frame": frame, "alpha_bbox": alpha_bbox(rgba)})
            alpha_sums.append(sum(rgba[3::4]))
    if len({tuple(sample["alpha_bbox"]) for sample in boxes}) != len(sample_indices):
        raise ValueError("Subject alpha must visibly move across the sequence")
    w, h, palette = read_png(root / "color-map.png")
    if (w, h) != (WIDTH, HEIGHT) or not all(a == 255 for a in palette[3::4]):
        raise ValueError("Static color map must be full-size and opaque")
    return {"verified_png_files": FRAME_COUNT * 2 + 1,
            "dimensions": [WIDTH, HEIGHT], "alpha_motion_samples": boxes,
            "sample_alpha_sums": alpha_sums}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    default = Path(__file__).resolve().parents[1] / "test-output" / "fixtures"
    parser.add_argument("--output", type=Path, default=default)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    root = args.output.resolve()
    if not args.verify_only:
        root.mkdir(parents=True, exist_ok=True)
        for frame in range(FRAME_COUNT):
            rgba, luma = make_frame(frame)
            name = f"frame_{frame:04d}.png"
            write_png(root / "rgba" / name, rgba, WIDTH, HEIGHT)
            write_png(root / "luma" / name, luma, WIDTH, HEIGHT)
        palette = bytearray(WIDTH * HEIGHT * 4)
        for y in range(HEIGHT):
            for x in range(WIDTH):
                rgb = map_color(x + 0.5, y + 0.5)
                index = (y * WIDTH + x) * 4
                palette[index:index + 4] = bytes((*[round(255 * c) for c in rgb], 255))
        write_png(root / "color-map.png", palette, WIDTH, HEIGHT)
        metadata = {
            "kind": "deterministic_algorithmic_geometry_test_fixture",
            "description_zh": "算法几何测试素材，用于验证发射、出生携色、序列像素运动拖尾和外部亮度遮罩；不是最终美术效果或画质目标。",
            "width": WIDTH, "height": HEIGHT, "fps": FPS, "frame_count": FRAME_COUNT,
            "duration_seconds": FRAME_COUNT / FPS,
            "first_frame": 0, "last_frame": FRAME_COUNT - 1,
            "rgba_sequence": "rgba/frame_%04d.png",
            "rgba_encoding": "8-bit straight RGBA PNG, transparent background, sRGB",
            "luma_sequence": "luma/frame_%04d.png",
            "luma_encoding": "8-bit opaque RGBA PNG, equal RGB channels, black background, sRGB",
            "color_map": "color-map.png",
            "motion": "Subject moves and rotates inside fixed 640x360 pixel frames; no AE layer transform keyframes are required.",
            "ae_import": "Import each sequence from frame_0000.png with PNG Sequence enabled; interpret footage at 25 fps. Keep both layers aligned at the same start time and scale.",
            "generator": "scripts/make-fixtures.py (Python standard library only)",
        }
        (root / "README.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report = verify(root)
    report["output"] = str(root)
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
