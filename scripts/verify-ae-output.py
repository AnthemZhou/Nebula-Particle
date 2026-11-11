#!/usr/bin/env python3
"""Decode AE output converted with sips, check actual pixels, and record evidence.

No third-party dependency. Supported PNGs: non-interlaced 8-bit RGB/RGBA.
PNG conversion is for inspection; AE's project bit depth is recorded separately.
"""
from pathlib import Path
import hashlib
import json
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'test-output'

def png(path):
    raw = Path(path).read_bytes()
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    at, parts = 8, []
    while at < len(raw):
        n = struct.unpack_from('>I', raw, at)[0]
        kind, chunk = raw[at+4:at+8], raw[at+8:at+8+n]
        assert zlib.crc32(kind + chunk) & 0xffffffff == struct.unpack_from('>I', raw, at+8+n)[0]
        if kind == b'IHDR':
            w, h, bits, mode, compression, filtering, interlace = struct.unpack('>IIBBBBB', chunk)
            assert bits == 8 and mode in (2, 6) and interlace == 0
        elif kind == b'IDAT':
            parts.append(chunk)
        at += 12+n
    channels = 4 if mode == 6 else 3
    stride = w*channels
    encoded = zlib.decompress(b''.join(parts))
    previous = bytearray(stride)
    rgba = bytearray()
    pos = 0
    def paeth(a, b, c):
        estimate = a+b-c
        da, db, dc = abs(estimate-a), abs(estimate-b), abs(estimate-c)
        return a if da <= db and da <= dc else (b if db <= dc else c)
    for _ in range(h):
        kind = encoded[pos]; pos += 1
        row = bytearray(encoded[pos:pos+stride]); pos += stride
        for x in range(stride):
            left = row[x-channels] if x >= channels else 0
            above = previous[x]
            diagonal = previous[x-channels] if x >= channels else 0
            predictor = (0, left, above, (left+above)//2, paeth(left, above, diagonal))[kind]
            row[x] = (row[x]+predictor) & 255
        if channels == 4:
            rgba.extend(row)
        else:
            for x in range(w): rgba.extend(row[x*3:x*3+3] + b'\xff')
        previous = row
    assert len(rgba) == w*h*4
    return w, h, bytes(rgba)

def main():
    paths = {
        'alpha': OUT/'ae-preview-alpha.png',
        'damping': OUT/'ae-preview-damping.png',
        'external_mask': OUT/'ae-preview-mask.png',
        'repeat': OUT/'ae-preview-repeat.png',
    }
    decoded = {key: png(path) for key, path in paths.items()}
    for key, (w, h, pixels) in decoded.items():
        assert (w, h) == (640, 360), key
        assert sum(a > 0 for a in pixels[3::4]) > 100, key+' empty'
        assert min(pixels[3::4]) == 0, key+' lost transparency'
    # The stock PSD template exports 8-bit files even for the 16-bpc project.
    # Inspect the observed small quantization variation separately, and use
    # the all-8-bpc pipeline below for the strict equality assertion.
    delta16 = max(abs(x-y) for x,y in zip(decoded['alpha'][2], decoded['repeat'][2]))
    assert delta16 <= 2, '16-to-8 export repeat exceeded two code values'
    a, b = decoded['alpha'][2], decoded['damping'][2]
    changes = sum(a[i:i+4] != b[i:i+4] for i in range(0, len(a), 4))
    assert changes > 100, 'Damping did not change the rendered trajectory'
    source = png(OUT/'fixtures/rgba/frame_0040.png')[2]
    outside = sum(a[i+3] > 0 and source[i+3] == 0 for i in range(0, len(a), 4))
    assert outside > 100, 'No particles outside the current source silhouette'
    report = {
        'native_frame_size': [640, 360],
        'repeat16_pixels_identical': decoded['alpha'] == decoded['repeat'],
        'repeat16_max_code_difference_in_8bit_export': delta16,
        'damping_changed_pixels': changes,
        'particle_pixels_outside_current_source': outside,
        'hashes': {key: hashlib.sha256(value[2]).hexdigest() for key, value in decoded.items()},
        'notes': 'Decoded PNG inspection copies; actual AE projects rendered at 8/16 bpc. Export template itself uses 8-bit PSD.'
    }
    for filename, key in [('ae-preview-8.png','eight_bit'), ('ae-preview-stop.png','stop'), ('ae-preview-after-end.png','after_end')]:
        file = OUT/filename
        if file.exists():
            _, _, p = png(file)
            count = sum(v > 0 for v in p[3::4])
            report[key+'_visible_pixels'] = count
            if key == 'stop': assert count == 0, 'Particles remain after stop time + lifetime'
            else: assert count > 0, key+' unexpectedly empty'
    first8 = OUT/'ae-preview-8.png'
    repeat8 = OUT/'ae-preview-8-repeat.png'
    if first8.exists() and repeat8.exists():
        assert png(first8) == png(repeat8), '8-bpc repeat differed after a time jump'
        report['repeat8_pixels_identical'] = True
    (OUT/'pixel-verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    main()
