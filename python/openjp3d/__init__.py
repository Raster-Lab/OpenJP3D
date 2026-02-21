# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

"""
openjp3d — Python bindings for the OpenJP3D volumetric codec library.

Quick-start
-----------
>>> import numpy as np
>>> import openjp3d
>>>
>>> # Create a synthetic 3-D volume (D, H, W)
>>> vol = np.random.randint(0, 256, (16, 32, 32), dtype=np.uint8)
>>>
>>> # Encode to a JP3D codestream (bytes)
>>> codestream = openjp3d.encode(vol)
>>>
>>> # Decode back to a NumPy array
>>> decoded = openjp3d.decode(codestream)
>>> assert np.array_equal(vol, decoded)  # lossless by default

Public API
----------
- :func:`encode`            — Encode a NumPy array to JP3D bytes.
- :func:`decode`            — Decode JP3D bytes to a NumPy array.
- :func:`transcode_to_ht`   — Transcode a JP3D codestream to HTJ2K.
- :func:`get_version`       — Return the native library version string.
- :class:`EncodeParams`     — Encoder parameter dataclass.
- Constants:
    ``FILTER_53``, ``FILTER_97``, ``USE_HTJ2K``,
    ``CS_UNKNOWN``, ``CS_SRGB``, ``CS_GRAY``, ``CS_YUV``.
"""

from __future__ import annotations

import ctypes
import warnings
from dataclasses import dataclass, field
from typing import Callable, Optional

try:
    import numpy as np
    _HAS_NUMPY = True
except ImportError:  # pragma: no cover
    _HAS_NUMPY = False

from ._bindings import (
    OPJ_EncParams,
    OPJ_DecParams,
    MsgCallbackType,
    OPJ_JP3D_TRUE,
    OPJ_JP3D_FILTER_53,
    OPJ_JP3D_FILTER_97,
    OPJ_JP3D_USE_HTJ2K,
    OPJ_JP3D_CS_UNKNOWN,
    OPJ_JP3D_CS_SRGB,
    OPJ_JP3D_CS_GRAY,
    OPJ_JP3D_CS_YUV,
    get_bindings,
)

__version__ = "1.0.0"
__all__ = [
    "encode",
    "decode",
    "transcode_to_ht",
    "get_version",
    "EncodeParams",
    "FILTER_53",
    "FILTER_97",
    "USE_HTJ2K",
    "CS_UNKNOWN",
    "CS_SRGB",
    "CS_GRAY",
    "CS_YUV",
]

# ---------------------------------------------------------------------------
# Re-exported constants
# ---------------------------------------------------------------------------
FILTER_53  = OPJ_JP3D_FILTER_53
FILTER_97  = OPJ_JP3D_FILTER_97
USE_HTJ2K  = OPJ_JP3D_USE_HTJ2K
CS_UNKNOWN = OPJ_JP3D_CS_UNKNOWN
CS_SRGB    = OPJ_JP3D_CS_SRGB
CS_GRAY    = OPJ_JP3D_CS_GRAY
CS_YUV     = OPJ_JP3D_CS_YUV


# ---------------------------------------------------------------------------
# Encoder parameter dataclass
# ---------------------------------------------------------------------------

@dataclass
class EncodeParams:
    """
    Parameters for :func:`encode`.

    All fields mirror ``opj_jp3d_enc_params_t`` with Pythonic defaults.

    Parameters
    ----------
    tile_width, tile_height, tile_depth : int
        Tile dimensions.  ``0`` means use the whole volume (default).
    num_resolutions_x/y/z : int
        Number of DWT decomposition levels per axis (default 3).
    cblk_width, cblk_height, cblk_depth : int
        Code-block dimensions (default 4×4×4).
    filter : int
        ``FILTER_53`` (lossless, default) or ``FILTER_97`` (lossy).
    num_layers : int
        Number of quality layers (default 1).
    target_rate : float
        Target bits/sample.  ``0.0`` means lossless (default).
    use_htj2k : int
        Set to ``USE_HTJ2K`` to enable the HT block coder (default off).
    verbose : bool
        Enable codec info messages (default ``False``).
    """

    tile_width:        int   = 0
    tile_height:       int   = 0
    tile_depth:        int   = 0
    num_resolutions_x: int   = 3
    num_resolutions_y: int   = 3
    num_resolutions_z: int   = 3
    cblk_width:        int   = 4
    cblk_height:       int   = 4
    cblk_depth:        int   = 4
    filter:            int   = FILTER_53
    num_layers:        int   = 1
    target_rate:       float = 0.0
    use_htj2k:         int   = 0
    verbose:           bool  = False

    def _to_c(self) -> OPJ_EncParams:
        """Convert to the ctypes ``OPJ_EncParams`` structure."""
        p = OPJ_EncParams()
        p.tile_width        = self.tile_width
        p.tile_height       = self.tile_height
        p.tile_depth        = self.tile_depth
        p.num_resolutions_x = self.num_resolutions_x
        p.num_resolutions_y = self.num_resolutions_y
        p.num_resolutions_z = self.num_resolutions_z
        p.cblk_width        = self.cblk_width
        p.cblk_height       = self.cblk_height
        p.cblk_depth        = self.cblk_depth
        p.filter            = self.filter
        p.num_layers        = self.num_layers
        p.target_rate       = self.target_rate
        p.use_htj2k         = self.use_htj2k
        p.verbose           = int(self.verbose)
        return p


# ---------------------------------------------------------------------------
# Message callback helper
# ---------------------------------------------------------------------------

def _make_callback(
    on_message: Optional[Callable[[int, str], None]]
) -> Optional[MsgCallbackType]:
    """
    Wrap *on_message* in a C-callable function object, or return None.

    The returned object must be kept alive for the duration of the C call.
    """
    if on_message is None:
        return None

    def _cb(level: int, message: bytes, _data: int) -> None:  # type: ignore[override]
        text = message.decode("utf-8", errors="replace") if message else ""
        on_message(level, text)

    return MsgCallbackType(_cb)


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def get_version() -> str:
    """Return the native OpenJP3D library version string (e.g. ``"1.0.0"``)."""
    lib = get_bindings()
    return lib.opj_jp3d_get_version().decode("ascii")


def encode(
    volume,  # numpy.ndarray or list-of-lists-of-lists
    params: Optional[EncodeParams] = None,
    *,
    on_message: Optional[Callable[[int, str], None]] = None,
) -> bytes:
    """
    Encode a 3-D volume to a JP3D codestream.

    Parameters
    ----------
    volume : array-like, shape (D, H, W) or (D, H, W, C)
        Voxel data.  Supported dtypes: ``uint8``, ``uint16``, ``int8``,
        ``int16``, ``int32``.  The array is treated as component-major
        when it has 4 dimensions (last axis = components); a 3-D array
        is treated as a single-component volume.
    params : EncodeParams, optional
        Encoder parameters.  Defaults are used when omitted.
    on_message : callable(level, message), optional
        Receives codec messages.  *level* is one of ``0`` (info),
        ``1`` (warning), ``2`` (error).

    Returns
    -------
    bytes
        Raw JP3D codestream.

    Raises
    ------
    ImportError
        If NumPy is not installed.
    TypeError
        If *volume* has an unsupported dtype.
    RuntimeError
        If the codec reports an error.
    """
    if not _HAS_NUMPY:
        raise ImportError("NumPy is required for openjp3d.encode()")  # pragma: no cover

    arr = np.asarray(volume)

    # Determine shape: (D, H, W) or (D, H, W, C)
    if arr.ndim == 3:
        d, h, w = arr.shape
        numcomps = 1
        arr = arr[:, :, :, np.newaxis]  # → (D, H, W, 1)
    elif arr.ndim == 4:
        d, h, w, numcomps = arr.shape
    else:
        raise ValueError(
            f"Expected a 3-D (D,H,W) or 4-D (D,H,W,C) array, got {arr.ndim}-D"
        )

    # Determine precision and signedness from dtype
    dtype_map = {
        np.dtype("uint8"):  (8,  0),
        np.dtype("int8"):   (8,  1),
        np.dtype("uint16"): (16, 0),
        np.dtype("int16"):  (16, 1),
        np.dtype("int32"):  (32, 1),
    }
    if arr.dtype not in dtype_map:
        raise TypeError(
            f"Unsupported dtype {arr.dtype}. "
            "Supported: uint8, int8, uint16, int16, int32."
        )
    prec, sgnd = dtype_map[arr.dtype]

    lib = get_bindings()

    # Create volume
    vol_ptr = lib.opj_jp3d_create_volume(numcomps, w, h, d, prec, sgnd)
    if not vol_ptr:
        raise RuntimeError("opj_jp3d_create_volume() returned NULL")

    try:
        # Populate component data
        for c in range(numcomps):
            comp = vol_ptr.contents.comps[c]
            # comp.data is a pre-allocated int32* (w*h*d elements)
            comp_slice = arr[:, :, :, c]  # (D, H, W)
            flat = comp_slice.ravel().astype(np.int32)
            # Iterate z, y, x order (data[z*w*h + y*w + x])
            ctypes.memmove(
                comp.data,
                flat.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
                flat.nbytes,
            )

        # Build C params
        if params is None:
            params = EncodeParams()
        c_params = params._to_c()

        # Callback
        cb = _make_callback(on_message)
        cb_arg = cb if cb is not None else ctypes.cast(None, MsgCallbackType)

        out_ptr  = ctypes.c_void_p(0)
        out_size = ctypes.c_size_t(0)

        ok = lib.opj_jp3d_encode(
            vol_ptr,
            ctypes.byref(c_params),
            ctypes.byref(out_ptr),
            ctypes.byref(out_size),
            cb_arg,
            None,
        )
        if ok != OPJ_JP3D_TRUE:
            raise RuntimeError("opj_jp3d_encode() failed")

        # Copy codestream into Python bytes
        n = out_size.value
        buf = (ctypes.c_uint8 * n).from_address(out_ptr.value)
        result = bytes(buf)
        lib.opj_jp3d_free(ctypes.c_void_p(out_ptr.value))
        return result

    finally:
        lib.opj_jp3d_destroy_volume(vol_ptr)


def decode(
    data: bytes,
    *,
    verbose: bool = False,
    on_message: Optional[Callable[[int, str], None]] = None,
) -> "np.ndarray":
    """
    Decode a JP3D codestream to a NumPy array.

    Parameters
    ----------
    data : bytes
        Raw JP3D codestream.
    verbose : bool, optional
        Enable codec info messages (default ``False``).
    on_message : callable(level, message), optional
        Receives codec messages.

    Returns
    -------
    numpy.ndarray
        Shape ``(D, H, W)`` for single-component volumes or
        ``(D, H, W, C)`` for multi-component volumes.
        Dtype is chosen to match the encoded bit-depth and signedness:
        ``uint8``, ``int8``, ``uint16``, ``int16``, or ``int32``.

    Raises
    ------
    ImportError
        If NumPy is not installed.
    RuntimeError
        If the codec reports an error.
    """
    if not _HAS_NUMPY:
        raise ImportError("NumPy is required for openjp3d.decode()")  # pragma: no cover

    lib = get_bindings()

    dec_params = OPJ_DecParams()
    dec_params.verbose = int(verbose)

    cb = _make_callback(on_message)
    cb_arg = cb if cb is not None else ctypes.cast(None, MsgCallbackType)

    vol_ptr = lib.opj_jp3d_decode(
        data,
        len(data),
        ctypes.byref(dec_params),
        cb_arg,
        None,
    )
    if not vol_ptr:
        raise RuntimeError("opj_jp3d_decode() returned NULL — decoding failed")

    try:
        vol = vol_ptr.contents
        numcomps = vol.numcomps
        comp0    = vol.comps[0]
        w, h, d  = comp0.w, comp0.h, comp0.d
        prec     = comp0.prec
        sgnd     = comp0.sgnd

        # Choose NumPy dtype
        if prec <= 8:
            out_dtype = np.dtype("int8") if sgnd else np.dtype("uint8")
        elif prec <= 16:
            out_dtype = np.dtype("int16") if sgnd else np.dtype("uint16")
        else:
            out_dtype = np.dtype("int32")

        n_voxels = w * h * d
        result = np.empty((d, h, w, numcomps), dtype=np.int32)

        for c in range(numcomps):
            comp = vol.comps[c]
            flat = np.frombuffer(
                (ctypes.c_int32 * n_voxels).from_address(
                    ctypes.addressof(comp.data.contents)
                ),
                dtype=np.int32,
                count=n_voxels,
            ).copy()
            result[:, :, :, c] = flat.reshape(d, h, w)

        # Convert dtype
        result = result.astype(out_dtype)

        # Squeeze single-component to (D, H, W)
        if numcomps == 1:
            result = result[:, :, :, 0]

        return result

    finally:
        lib.opj_jp3d_destroy_volume(vol_ptr)


def transcode_to_ht(
    data: bytes,
    params: Optional[EncodeParams] = None,
    *,
    on_message: Optional[Callable[[int, str], None]] = None,
) -> bytes:
    """
    Losslessly transcode a JP3D codestream from EBCOT to HTJ2K.

    Parameters
    ----------
    data : bytes
        Source JP3D codestream (EBCOT or HTJ2K).
    params : EncodeParams, optional
        Encoder parameters for the output stream.  The ``use_htj2k`` flag
        is forced to ``USE_HTJ2K`` regardless of the value supplied.
    on_message : callable(level, message), optional
        Receives codec messages.

    Returns
    -------
    bytes
        Transcoded JP3D codestream using the HT block coder.

    Raises
    ------
    RuntimeError
        If transcoding fails.
    """
    lib = get_bindings()

    if params is not None:
        c_params = params._to_c()
        c_params.use_htj2k = USE_HTJ2K
        p_params = ctypes.byref(c_params)
    else:
        p_params = ctypes.cast(None, ctypes.POINTER(OPJ_EncParams))

    cb = _make_callback(on_message)
    cb_arg = cb if cb is not None else ctypes.cast(None, MsgCallbackType)

    out_ptr  = ctypes.c_void_p(0)
    out_size = ctypes.c_size_t(0)

    ok = lib.opj_jp3d_transcode_to_ht(
        data,
        len(data),
        p_params,
        ctypes.byref(out_ptr),
        ctypes.byref(out_size),
        cb_arg,
        None,
    )
    if ok != OPJ_JP3D_TRUE:
        raise RuntimeError("opj_jp3d_transcode_to_ht() failed")

    n   = out_size.value
    buf = (ctypes.c_uint8 * n).from_address(out_ptr.value)
    result = bytes(buf)
    lib.opj_jp3d_free(ctypes.c_void_p(out_ptr.value))
    return result
