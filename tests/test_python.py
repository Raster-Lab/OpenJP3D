# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

"""
Python binding tests for the openjp3d package.

Run with:
    OPENJP3D_LIBRARY=/path/to/libopenjp3d.so \
    PYTHONPATH=/path/to/repo/python \
    pytest tests/test_python.py -v
"""

import os
import sys
import ctypes

import pytest

# ---------------------------------------------------------------------------
# Check that the shared library is available before importing openjp3d.
# Tests are skipped when the library cannot be found (e.g. BUILD_SHARED_LIBS=OFF
# or library not yet built).
# ---------------------------------------------------------------------------
_LIB_AVAILABLE = bool(os.environ.get("OPENJP3D_LIBRARY"))
if not _LIB_AVAILABLE:
    # Also try ctypes.util
    import ctypes.util as _util
    _LIB_AVAILABLE = bool(_util.find_library("openjp3d"))

pytestmark = pytest.mark.skipif(
    not _LIB_AVAILABLE,
    reason="openjp3d shared library not found; set OPENJP3D_LIBRARY",
)

# ---------------------------------------------------------------------------
# Imports — done lazily so collection doesn't fail without the library
# ---------------------------------------------------------------------------
try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

requires_numpy = pytest.mark.skipif(not HAS_NUMPY, reason="NumPy not installed")


@pytest.fixture(scope="session")
def lib():
    """Return the loaded openjp3d module."""
    import openjp3d
    return openjp3d


# ===========================================================================
# 1. Library loading & version
# ===========================================================================

class TestVersion:
    def test_get_version_returns_string(self, lib):
        v = lib.get_version()
        assert isinstance(v, str)
        assert v  # non-empty

    def test_version_format(self, lib):
        v = lib.get_version()
        parts = v.split(".")
        assert len(parts) == 3, f"Expected MAJOR.MINOR.PATCH, got {v!r}"
        for p in parts:
            assert p.isdigit(), f"Version part {p!r} is not numeric"

    def test_module_version_attribute(self, lib):
        assert hasattr(lib, "__version__")
        assert isinstance(lib.__version__, str)


# ===========================================================================
# 2. Constants
# ===========================================================================

class TestConstants:
    def test_filter_constants(self, lib):
        assert lib.FILTER_53 == 0
        assert lib.FILTER_97 == 1

    def test_htj2k_constant(self, lib):
        assert lib.USE_HTJ2K == 1

    def test_colorspace_constants(self, lib):
        assert lib.CS_UNKNOWN == 0
        assert lib.CS_SRGB    == 1
        assert lib.CS_GRAY    == 2
        assert lib.CS_YUV     == 3


# ===========================================================================
# 3. EncodeParams dataclass
# ===========================================================================

class TestEncodeParams:
    def test_default_construction(self, lib):
        p = lib.EncodeParams()
        assert p.tile_width  == 0
        assert p.tile_height == 0
        assert p.tile_depth  == 0
        assert p.filter      == lib.FILTER_53
        assert p.target_rate == 0.0
        assert p.use_htj2k   == 0
        assert p.verbose     is False

    def test_custom_construction(self, lib):
        p = lib.EncodeParams(
            tile_width=32, tile_height=32, tile_depth=16,
            filter=lib.FILTER_97,
            target_rate=2.0,
            use_htj2k=lib.USE_HTJ2K,
        )
        assert p.tile_width  == 32
        assert p.filter      == lib.FILTER_97
        assert p.target_rate == pytest.approx(2.0)
        assert p.use_htj2k   == lib.USE_HTJ2K

    def test_to_c_conversion(self, lib):
        from openjp3d._bindings import OPJ_EncParams
        p = lib.EncodeParams(tile_width=64, filter=lib.FILTER_97, verbose=True)
        c = p._to_c()
        assert isinstance(c, OPJ_EncParams)
        assert c.tile_width == 64
        assert c.filter     == lib.FILTER_97
        assert c.verbose    == 1


# ===========================================================================
# 4. Encode / Decode round-trips
# ===========================================================================

@requires_numpy
class TestRoundTrip:

    def test_encode_returns_bytes(self, lib):
        vol = np.zeros((4, 4, 4), dtype=np.uint8)
        cs  = lib.encode(vol)
        assert isinstance(cs, bytes)
        assert len(cs) > 0

    def test_lossless_uint8_single_component(self, lib):
        rng = np.random.default_rng(0)
        vol = rng.integers(0, 256, (8, 16, 16), dtype=np.uint8)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.dtype == np.uint8
        assert out.shape == vol.shape
        assert np.array_equal(vol, out)

    def test_lossless_uint16_single_component(self, lib):
        rng = np.random.default_rng(1)
        vol = rng.integers(0, 65536, (4, 8, 8), dtype=np.uint16)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.dtype == np.uint16
        assert out.shape == vol.shape
        assert np.array_equal(vol, out)

    def test_lossless_int8(self, lib):
        rng = np.random.default_rng(2)
        vol = rng.integers(-128, 128, (4, 8, 8), dtype=np.int8)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.dtype == np.int8
        assert np.array_equal(vol, out)

    def test_lossless_int16(self, lib):
        rng = np.random.default_rng(3)
        vol = rng.integers(-32768, 32768, (4, 8, 8), dtype=np.int16)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.dtype == np.int16
        assert np.array_equal(vol, out)

    def test_lossless_int32(self, lib):
        rng = np.random.default_rng(4)
        vol = rng.integers(-2**20, 2**20, (4, 8, 8), dtype=np.int32)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.dtype == np.int32
        assert np.array_equal(vol, out)

    def test_all_zeros(self, lib):
        vol = np.zeros((8, 8, 8), dtype=np.uint8)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert np.array_equal(vol, out)

    def test_all_ones(self, lib):
        vol = np.ones((8, 8, 8), dtype=np.uint8)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert np.array_equal(vol, out)

    def test_multicomponent_rgb(self, lib):
        rng = np.random.default_rng(5)
        vol = rng.integers(0, 256, (4, 8, 8, 3), dtype=np.uint8)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.shape == vol.shape
        assert np.array_equal(vol, out)

    def test_multicomponent_4ch(self, lib):
        rng = np.random.default_rng(6)
        vol = rng.integers(0, 256, (4, 8, 8, 4), dtype=np.uint8)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.shape == vol.shape
        assert np.array_equal(vol, out)

    def test_single_slice(self, lib):
        """Depth-1 (single slice) volume."""
        rng = np.random.default_rng(7)
        vol = rng.integers(0, 256, (1, 16, 16), dtype=np.uint8)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.shape == vol.shape
        assert np.array_equal(vol, out)

    def test_codestream_starts_with_soc(self, lib):
        """JP3D codestream must begin with the SOC marker (0xFF4F)."""
        vol = np.zeros((4, 4, 4), dtype=np.uint8)
        cs  = lib.encode(vol)
        assert cs[0] == 0xFF
        assert cs[1] == 0x4F

    def test_encode_non_square(self, lib):
        rng = np.random.default_rng(8)
        vol = rng.integers(0, 256, (3, 7, 11), dtype=np.uint8)
        cs  = lib.encode(vol)
        out = lib.decode(cs)
        assert out.shape == vol.shape
        assert np.array_equal(vol, out)


# ===========================================================================
# 5. EncodeParams applied to encode/decode
# ===========================================================================

@requires_numpy
class TestEncodeParamsInUse:

    def test_htj2k_round_trip(self, lib):
        rng = np.random.default_rng(9)
        vol = rng.integers(0, 256, (4, 8, 8), dtype=np.uint8)
        p   = lib.EncodeParams(use_htj2k=lib.USE_HTJ2K)
        cs  = lib.encode(vol, p)
        out = lib.decode(cs)
        assert np.array_equal(vol, out)

    def test_lossy_filter_97(self, lib):
        """Lossy encode + decode should not error (PSNR not checked here)."""
        rng = np.random.default_rng(10)
        vol = rng.integers(0, 256, (8, 16, 16), dtype=np.uint8)
        p   = lib.EncodeParams(filter=lib.FILTER_97, target_rate=2.0)
        cs  = lib.encode(vol, p)
        out = lib.decode(cs)
        assert out.shape == vol.shape

    def test_tiled_encode(self, lib):
        """Tiled encoding should produce a valid lossless round-trip."""
        rng = np.random.default_rng(11)
        vol = rng.integers(0, 256, (8, 16, 16), dtype=np.uint8)
        p   = lib.EncodeParams(tile_width=8, tile_height=8, tile_depth=4)
        cs  = lib.encode(vol, p)
        out = lib.decode(cs)
        assert np.array_equal(vol, out)

    def test_verbose_flag_no_crash(self, lib):
        """Verbose mode must not crash (messages may go to stderr)."""
        vol = np.zeros((4, 4, 4), dtype=np.uint8)
        p   = lib.EncodeParams(verbose=True)
        cs  = lib.encode(vol, p)
        assert len(cs) > 0


# ===========================================================================
# 6. Message callback
# ===========================================================================

@requires_numpy
class TestMessageCallback:

    def test_no_callback(self, lib):
        vol = np.zeros((4, 4, 4), dtype=np.uint8)
        cs  = lib.encode(vol, on_message=None)
        assert len(cs) > 0

    def test_callback_receives_messages(self, lib):
        messages = []

        def _cb(level, msg):
            messages.append((level, msg))

        vol = np.zeros((4, 4, 4), dtype=np.uint8)
        p   = lib.EncodeParams(verbose=True)
        lib.encode(vol, p, on_message=_cb)
        # With verbose=True at least one info message should be emitted;
        # if not, the callback must still not crash.

    def test_decode_callback(self, lib):
        messages = []

        def _cb(level, msg):
            messages.append((level, msg))

        vol = np.zeros((4, 4, 4), dtype=np.uint8)
        cs  = lib.encode(vol)
        lib.decode(cs, verbose=True, on_message=_cb)


# ===========================================================================
# 7. transcode_to_ht
# ===========================================================================

@requires_numpy
class TestTranscodeToHT:

    def test_transcode_produces_bytes(self, lib):
        vol = np.zeros((4, 4, 4), dtype=np.uint8)
        cs  = lib.encode(vol)
        ht  = lib.transcode_to_ht(cs)
        assert isinstance(ht, bytes)
        assert len(ht) > 0

    def test_transcode_round_trip(self, lib):
        rng = np.random.default_rng(12)
        vol = rng.integers(0, 256, (4, 8, 8), dtype=np.uint8)
        cs  = lib.encode(vol)
        ht  = lib.transcode_to_ht(cs)
        out = lib.decode(ht)
        assert np.array_equal(vol, out)

    def test_transcode_with_params(self, lib):
        rng = np.random.default_rng(13)
        vol = rng.integers(0, 256, (4, 8, 8), dtype=np.uint8)
        cs  = lib.encode(vol)
        p   = lib.EncodeParams()
        ht  = lib.transcode_to_ht(cs, p)
        out = lib.decode(ht)
        assert np.array_equal(vol, out)


# ===========================================================================
# 8. Error handling
# ===========================================================================

@requires_numpy
class TestErrorHandling:

    def test_decode_invalid_data_raises(self, lib):
        with pytest.raises(RuntimeError):
            lib.decode(b"not a valid jp3d codestream")

    def test_encode_wrong_ndim_raises(self, lib):
        with pytest.raises(ValueError):
            lib.encode(np.zeros((4, 4), dtype=np.uint8))  # 2-D

    def test_encode_wrong_ndim_5d_raises(self, lib):
        with pytest.raises(ValueError):
            lib.encode(np.zeros((2, 2, 2, 2, 2), dtype=np.uint8))  # 5-D

    def test_encode_unsupported_dtype_raises(self, lib):
        with pytest.raises(TypeError):
            lib.encode(np.zeros((4, 4, 4), dtype=np.float32))


# ===========================================================================
# 9. Bindings module internals
# ===========================================================================

class TestBindings:

    def test_bindings_structures_importable(self):
        from openjp3d._bindings import (
            OPJ_VolumeComp,
            OPJ_Volume,
            OPJ_EncParams,
            OPJ_DecParams,
            MsgCallbackType,
        )
        import ctypes
        assert issubclass(OPJ_VolumeComp, ctypes.Structure)
        assert issubclass(OPJ_Volume,     ctypes.Structure)
        assert issubclass(OPJ_EncParams,  ctypes.Structure)
        assert issubclass(OPJ_DecParams,  ctypes.Structure)

    def test_get_bindings_idempotent(self):
        from openjp3d._bindings import get_bindings
        lib1 = get_bindings()
        lib2 = get_bindings()
        assert lib1 is lib2

    def test_lib_loader_returns_same_object(self):
        from openjp3d._lib import get_lib
        lib1 = get_lib()
        lib2 = get_lib()
        assert lib1 is lib2
