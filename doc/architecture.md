# OpenJP3D Architecture

This document describes the internal module structure, data flow, and
extension points of the OpenJP3D library.

## High-Level Overview

OpenJP3D is organised into three independent libraries and a set of
command-line tools:

```
┌────────────────────────────────────────────────────┐
│                  CLI Tools (src/bin/jp3d/)          │
│  opj_jp3d_compress  opj_jp3d_decompress            │
│  opj_jp3d_dump      opj_jp3d_transcode             │
│  opj_jpip3d_server                                 │
├────────────────────┬───────────────────────────────┤
│  openjpip3d        │       (optional)              │
│  JPIP 3-D Streaming│                               │
├────────────────────┤                               │
│              openjp3d — Core JP3D Codec             │
│  Volume I/O │ 3-D DWT │ EBCOT/HT │ Codestream     │
└────────────────────────────────────────────────────┘
```

- **openjp3d** — Core codec implementing ISO/IEC 15444-10 (JP3D). Always
  built.
- **openjpip3d** — JPIP volumetric streaming (Part 9 extended to 3-D).
  Optional; links against openjp3d.
- **CLI tools** — Command-line utilities for encoding, decoding, inspecting,
  and transcoding JP3D codestreams. Optional.

## Core Codec Modules (src/lib/openjp3d/)

### Public API — `openjp3d.h` / `openjp3d.c`

The single public header exposes the encoder, decoder, transcoder, volume
lifecycle functions, and memory management wrappers. All public symbols use
the `opj_jp3d_` prefix.

Entry points:

| Function | Purpose |
|----------|---------|
| `opj_jp3d_encode()` | Encode a volume to a JP3D codestream |
| `opj_jp3d_decode()` | Decode a JP3D codestream to a volume |
| `opj_jp3d_transcode_to_ht()` | Transcode EBCOT → HTJ2K |
| `opj_jp3d_create_volume()` | Allocate a new volume |
| `opj_jp3d_destroy_volume()` | Free a volume |

### Volume — `opj_volume.h` / `opj_volume.c`

Defines `opj_volume_t` and `opj_volume_comp_t` data structures. A volume
contains one or more components, each with width × height × depth samples
stored as a flat `int32_t` array in `data[z * w * h + y * w + x]` order.

### 3-D Discrete Wavelet Transform — `opj_dwt3d.h` / `opj_dwt3d.c`

Separable 3-D DWT implementation with two filter modes:

- **5/3 integer lifting** (`OPJ_JP3D_FILTER_53`) — lossless, reversible.
- **9/7 float lifting** (`OPJ_JP3D_FILTER_97`) — lossy, irreversible.

The transform is applied separably along X, then Y, then Z. Each axis
supports configurable decomposition levels and symmetric boundary extension.

**SIMD dispatch**: The X-direction row transform is the primary SIMD target.
At runtime, `opj_cpu_features()` detects the available ISA and selects:

```
AVX2 → SSE4.1 → NEON → scalar (fallback)
```

SIMD implementations are in separate compilation units with per-file ISA
flags:

| File | ISA | Samples per word |
|------|-----|-----------------|
| `opj_dwt3d_avx2.c` | AVX2 | 8 × int32 |
| `opj_dwt3d_sse41.c` | SSE4.1 | 4 × int32 |
| `opj_dwt3d_neon.c` | NEON (AArch64) | 4 × int32 |

### Entropy Coding — EBCOT 3-D

- **Tier-1** (`opj_t1_3d.h` / `opj_t1_3d.c`) — Bit-plane coder with
  significance propagation, magnitude refinement, and cleanup passes.
  Operates on 3-D code-blocks.
- **Tier-2** (`opj_t2_3d.h` / `opj_t2_3d.c`) — Packet formation and layer
  coding. Assembles coded code-blocks into quality layers with
  post-compression rate control for lossy mode.

### HTJ2K Block Coder — `opj_ht3d.h` / `opj_ht3d.c`

Optional high-throughput block coder (Part 15 adapted for 3-D). Uses MEL
entropy coding for the significance stream and Exp-Golomb / MagSgn
variable-length coding for magnitudes and signs. Replaces the multi-pass
EBCOT Tier-1 coder when `OPJ_JP3D_USE_HTJ2K` is set.

### Codestream — `opj_cs3d.h` / `opj_cs3d.c`

JP3D codestream serialisation and parsing. Marker segments:

| Marker | Purpose |
|--------|---------|
| SOC | Start of codestream |
| SIZ3D | Volume extents, tile grid, component info |
| COD3D | Coding parameters (filter, resolutions, code-block sizes, HTJ2K flag) |
| QCD3D | Quantisation (target bit-rate) |
| SOT | Start of tile |
| SOD | Start of data |
| EOC | End of codestream |

### Tile Codec — `opj_tcd3d.h` / `opj_tcd3d.c`

Tile-level encode/decode orchestration. Manages tile partitioning,
per-tile DWT, and entropy coding. Supports optional OpenMP parallelism
at the tile level.

### Raw I/O — `opj_raw_io.h` / `opj_raw_io.c`

Reads and writes raw binary volumes (8/16/32-bit, signed/unsigned) from
flat files. Used by the CLI tools.

### Memory — `opj_mem.h` / `opj_mem.c`

Pluggable memory allocators (`opj_jp3d_malloc`, `opj_jp3d_free`,
`opj_jp3d_calloc`, `opj_jp3d_realloc`), consistent with OpenJPEG
conventions.

### CPU Detection — `opj_cpu.h` / `opj_cpu.c`

Runtime CPU feature detection via CPUID (x86-64) and compile-time
detection (AArch64 NEON). Result is memoised for zero-overhead dispatch.

## JPIP 3-D Streaming (src/lib/openjpip3d/)

### Public API — `openjpip3d.h` / `openjpip3d.c`

Extends JPIP (Part 9) to three dimensions with Z-axis parameters
(`fsiz3d`, `roff3d`, `rsiz3d`).

### Request Handling — `opj_jpip3d_request.h` / `opj_jpip3d_request.c`

Parses and serialises JPIP 3-D URL query strings. Supported keys:
`fsiz3d`, `roff3d`, `rsiz3d`, `comp`, `layers`, `level`, `sid`, `dataset`.

### Stream Formation — `opj_jpip3d_stream.h` / `opj_jpip3d_stream.c`

JPT-stream (tile-parts) and JPP-stream (precinct-parts) formation for
3-D codestream delivery.

### Cache Model — `opj_jpip3d_cache.h` / `opj_jpip3d_cache.c`

Server-side cache tracking of delivered precincts per session, avoiding
redundant data transmission.

### Server — `opj_jpip3d_server.h` / `opj_jpip3d_server.c`

Handles dataset loading, region extraction, request processing, and
per-session cache management.

### Client — `opj_jpip3d_client.h` / `opj_jpip3d_client.c`

Client session management — opening sessions, requesting volumetric
regions, and decoding received sub-volumes.

### Metadata — `opj_jpip3d_meta.h` / `opj_jpip3d_meta.c`

XML metadata wrapping, serialisation to JP2 XML boxes, and
deserialisation from box bytes.

## Data Flow

### Encoding

```
Volume (opj_volume_t)
  │
  ├─ Tile partitioning (opj_tcd3d)
  │   │
  │   ├─ Forward 3-D DWT (opj_dwt3d)
  │   │     X → Y → Z, with SIMD dispatch
  │   │
  │   ├─ Quantisation (opj_t1_3d / opj_ht3d)
  │   │
  │   ├─ Tier-1: EBCOT or HT block coding
  │   │
  │   └─ Tier-2: Packet formation + rate control
  │
  └─ Codestream assembly (opj_cs3d)
       SOC │ SIZ3D │ COD3D │ QCD3D │ SOT+SOD+data │ EOC
```

### Decoding

```
JP3D codestream (uint8_t *)
  │
  ├─ Codestream parsing (opj_cs3d)
  │     Marker extraction: SIZ3D, COD3D, QCD3D, SOT, SOD
  │
  ├─ Per-tile decoding (opj_tcd3d)
  │   │
  │   ├─ Tier-2: Packet parsing
  │   │
  │   ├─ Tier-1: EBCOT or HT block decoding (auto-detected)
  │   │
  │   ├─ Dequantisation
  │   │
  │   └─ Inverse 3-D DWT (opj_dwt3d)
  │         Z → Y → X, with SIMD dispatch
  │
  └─ Volume assembly (opj_volume)
```

### JPIP Streaming

```
Client                                          Server
  │                                               │
  ├─ Open session ──────────────────────────────► │
  │                                    session_id │
  ├─ Request region (fsiz3d, roff3d, rsiz3d) ───► │
  │                                               ├─ Check cache
  │                                               ├─ Extract region
  │                                               ├─ Form JPT/JPP stream
  │  ◄─────────────────── JPT/JPP response ──────┤
  ├─ Decode sub-volume                            │
  │                                               │
  ├─ Close session ─────────────────────────────► │
```

## Extension Points

- **Custom memory allocators**: Replace `opj_jp3d_malloc` / `opj_jp3d_free`
  implementations in `opj_mem.c` for integration with application-specific
  memory pools.
- **SIMD backends**: Add new ISA-specific DWT implementations following the
  pattern in `opj_dwt3d_avx2.c`. Register the new path in the dispatch
  table in `opj_dwt3d.c`.
- **Block coders**: The HTJ2K block coder is a drop-in replacement for EBCOT
  Tier-1. Additional block coding strategies can follow the same interface.
- **Stream types**: The JPIP module supports JPT and JPP stream types. New
  delivery modes can be added by implementing the stream formation interface
  in `opj_jpip3d_stream.c`.

## Build Configuration

Each library is an independent CMake target:

| Target | CMake Option | Default |
|--------|-------------|---------|
| `openjp3d` | `BUILD_JP3D` | ON |
| `openjpip3d` | `BUILD_JPIP_3D` | OFF |
| CLI tools | `BUILD_CLI_TOOLS` | ON |

The core codec is fully functional without HTJ2K or JPIP. Inter-module
coupling is minimal — openjpip3d links against openjp3d via the public API
only.
