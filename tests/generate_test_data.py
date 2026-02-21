#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Generate deterministic test data for OpenJP3D automated tests.

Each dataset uses a fixed seed or formula so results are reproducible across
platforms and runs.  No large binary files need to be checked into the
repository — this script (or equivalent C helpers) regenerates them on demand.

Usage:
    python3 tests/generate_test_data.py [output_directory]

Default output directory: /tmp/openjp3d_testdata
"""

import os
import struct
import random
import sys


def generate_all(out_dir):
    """Generate all test data files into *out_dir*."""
    os.makedirs(out_dir, exist_ok=True)

    # TD-001: 4x4x4 8-bit unsigned ramp (64 bytes)
    with open(os.path.join(out_dir, "td001_4x4x4_u8.raw"), "wb") as f:
        f.write(bytes(range(64)))

    # TD-002: 64x64x16 8-bit unsigned PRNG (seed 42)
    rng = random.Random(42)
    with open(os.path.join(out_dir, "td002_64x64x16_u8.raw"), "wb") as f:
        f.write(bytes(rng.getrandbits(8) for _ in range(64 * 64 * 16)))

    # TD-003: 4x4x4 16-bit signed ramp [-32768..32767] step 1024
    with open(os.path.join(out_dir, "td003_4x4x4_s16.raw"), "wb") as f:
        for i in range(64):
            f.write(struct.pack("<h", -32768 + i * 1024))

    # TD-004: 4x4x4 8-bit unsigned 3-component (RGB) ramp
    with open(os.path.join(out_dir, "td004_4x4x4_u8_rgb.raw"), "wb") as f:
        for c in range(3):
            f.write(bytes((i + c * 80) % 256 for i in range(64)))

    # TD-005: 4x4x1 8-bit unsigned single-slice
    with open(os.path.join(out_dir, "td005_4x4x1_u8.raw"), "wb") as f:
        f.write(bytes(range(16)))

    # TD-006: 4x4x4 8-bit signed ramp [-128..127]
    with open(os.path.join(out_dir, "td006_4x4x4_s8.raw"), "wb") as f:
        for i in range(64):
            f.write(struct.pack("b", -128 + i * 4))

    # TD-007: 128x128x32 8-bit unsigned PRNG (seed 99)
    rng7 = random.Random(99)
    with open(os.path.join(out_dir, "td007_128x128x32_u8.raw"), "wb") as f:
        chunk_size = 8192
        total = 128 * 128 * 32
        written = 0
        while written < total:
            n = min(chunk_size, total - written)
            f.write(bytes(rng7.getrandbits(8) for _ in range(n)))
            written += n

    # TD-008: 256x256x64 16-bit unsigned gradient + noise
    rng8 = random.Random(123)
    with open(os.path.join(out_dir, "td008_256x256x64_u16.raw"), "wb") as f:
        total = 256 * 256 * 64
        chunk_size = 4096
        written = 0
        while written < total:
            n = min(chunk_size, total - written)
            for idx in range(n):
                gradient = ((written + idx) * 65535) // total
                noise = rng8.randint(-256, 256)
                val = max(0, min(65535, gradient + noise))
                f.write(struct.pack("<H", val))
            written += n

    # TD-009: 64x64x16 8-bit unsigned 3-component PRNG (seed 7 per component)
    with open(os.path.join(out_dir, "td009_64x64x16_u8_rgb.raw"), "wb") as f:
        for c in range(3):
            rng9 = random.Random(7 + c)
            f.write(bytes(rng9.getrandbits(8) for _ in range(64 * 64 * 16)))

    # TD-010: 1x1x4 8-bit unsigned degenerate dimensions
    with open(os.path.join(out_dir, "td010_1x1x4_u8.raw"), "wb") as f:
        f.write(bytes([10, 20, 30, 40]))

    print(f"Generated 10 test data files in {out_dir}")


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/openjp3d_testdata"
    generate_all(out)
