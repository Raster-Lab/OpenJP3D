# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

"""ctypes structure definitions and function prototypes for the openjp3d C API."""

import ctypes
from ._lib import get_lib

# ---------------------------------------------------------------------------
# Basic types (matching openjp3d.h)
# ---------------------------------------------------------------------------

OPJ_JP3D_TRUE  = 1
OPJ_JP3D_FALSE = 0

OPJ_JP3D_FILTER_53 = 0
OPJ_JP3D_FILTER_97 = 1

OPJ_JP3D_USE_HTJ2K = 1

OPJ_JP3D_CS_UNKNOWN = 0
OPJ_JP3D_CS_SRGB    = 1
OPJ_JP3D_CS_GRAY    = 2
OPJ_JP3D_CS_YUV     = 3

# Message severity levels
OPJ_JP3D_MSG_INFO    = 0
OPJ_JP3D_MSG_WARNING = 1
OPJ_JP3D_MSG_ERROR   = 2

# ---------------------------------------------------------------------------
# C structure mirrors
# ---------------------------------------------------------------------------


class OPJ_VolumeComp(ctypes.Structure):
    """Mirror of opj_volume_comp_t."""

    _fields_ = [
        ("w",    ctypes.c_uint32),
        ("h",    ctypes.c_uint32),
        ("d",    ctypes.c_uint32),
        ("prec", ctypes.c_uint32),
        ("sgnd", ctypes.c_int32),
        ("dz",   ctypes.c_float),
        ("data", ctypes.POINTER(ctypes.c_int32)),
    ]


class OPJ_Volume(ctypes.Structure):
    """Mirror of opj_volume_t."""

    _fields_ = [
        ("numcomps",    ctypes.c_uint32),
        ("comps",       ctypes.POINTER(OPJ_VolumeComp)),
        ("x0",          ctypes.c_uint32),
        ("y0",          ctypes.c_uint32),
        ("z0",          ctypes.c_uint32),
        ("x1",          ctypes.c_uint32),
        ("y1",          ctypes.c_uint32),
        ("z1",          ctypes.c_uint32),
        ("color_space", ctypes.c_uint32),
    ]


class OPJ_EncParams(ctypes.Structure):
    """Mirror of opj_jp3d_enc_params_t."""

    _fields_ = [
        ("tile_width",        ctypes.c_uint32),
        ("tile_height",       ctypes.c_uint32),
        ("tile_depth",        ctypes.c_uint32),
        ("num_resolutions_x", ctypes.c_uint32),
        ("num_resolutions_y", ctypes.c_uint32),
        ("num_resolutions_z", ctypes.c_uint32),
        ("cblk_width",        ctypes.c_uint32),
        ("cblk_height",       ctypes.c_uint32),
        ("cblk_depth",        ctypes.c_uint32),
        ("filter",            ctypes.c_int32),
        ("num_layers",        ctypes.c_uint32),
        ("target_rate",       ctypes.c_float),
        ("use_htj2k",         ctypes.c_int32),
        ("verbose",           ctypes.c_int32),
    ]


class OPJ_DecParams(ctypes.Structure):
    """Mirror of opj_jp3d_dec_params_t."""

    _fields_ = [
        ("verbose", ctypes.c_int32),
    ]


# ---------------------------------------------------------------------------
# Callback type
# ---------------------------------------------------------------------------

MsgCallbackType = ctypes.CFUNCTYPE(
    None,                   # return void
    ctypes.c_int,           # opj_jp3d_msg_level_t
    ctypes.c_char_p,        # message
    ctypes.c_void_p,        # user data
)


# ---------------------------------------------------------------------------
# Function prototype registration
# ---------------------------------------------------------------------------

def _setup_prototypes(lib):
    """Attach argument/return-type annotations to all public API functions."""

    # opj_jp3d_get_version
    lib.opj_jp3d_get_version.restype  = ctypes.c_char_p
    lib.opj_jp3d_get_version.argtypes = []

    # opj_jp3d_malloc / calloc / realloc / free
    lib.opj_jp3d_malloc.restype  = ctypes.c_void_p
    lib.opj_jp3d_malloc.argtypes = [ctypes.c_size_t]

    lib.opj_jp3d_calloc.restype  = ctypes.c_void_p
    lib.opj_jp3d_calloc.argtypes = [ctypes.c_size_t, ctypes.c_size_t]

    lib.opj_jp3d_realloc.restype  = ctypes.c_void_p
    lib.opj_jp3d_realloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t]

    lib.opj_jp3d_free.restype  = None
    lib.opj_jp3d_free.argtypes = [ctypes.c_void_p]

    # opj_jp3d_create_volume
    lib.opj_jp3d_create_volume.restype  = ctypes.POINTER(OPJ_Volume)
    lib.opj_jp3d_create_volume.argtypes = [
        ctypes.c_uint32,  # numcomps
        ctypes.c_uint32,  # w
        ctypes.c_uint32,  # h
        ctypes.c_uint32,  # d
        ctypes.c_uint32,  # prec
        ctypes.c_int32,   # sgnd
    ]

    # opj_jp3d_destroy_volume
    lib.opj_jp3d_destroy_volume.restype  = None
    lib.opj_jp3d_destroy_volume.argtypes = [ctypes.POINTER(OPJ_Volume)]

    # opj_jp3d_set_default_encoder_parameters
    lib.opj_jp3d_set_default_encoder_parameters.restype  = None
    lib.opj_jp3d_set_default_encoder_parameters.argtypes = [
        ctypes.POINTER(OPJ_EncParams)
    ]

    # opj_jp3d_set_default_decoder_parameters
    lib.opj_jp3d_set_default_decoder_parameters.restype  = None
    lib.opj_jp3d_set_default_decoder_parameters.argtypes = [
        ctypes.POINTER(OPJ_DecParams)
    ]

    # opj_jp3d_encode
    lib.opj_jp3d_encode.restype  = ctypes.c_int32
    lib.opj_jp3d_encode.argtypes = [
        ctypes.POINTER(OPJ_Volume),     # volume
        ctypes.POINTER(OPJ_EncParams),  # params
        ctypes.POINTER(ctypes.c_void_p),  # out_data  (uint8_t **)
        ctypes.POINTER(ctypes.c_size_t),  # out_size
        MsgCallbackType,                  # callback
        ctypes.c_void_p,                  # callback_data
    ]

    # opj_jp3d_decode
    lib.opj_jp3d_decode.restype  = ctypes.POINTER(OPJ_Volume)
    lib.opj_jp3d_decode.argtypes = [
        ctypes.c_char_p,                  # data  (const uint8_t *)
        ctypes.c_size_t,                  # size
        ctypes.POINTER(OPJ_DecParams),    # params
        MsgCallbackType,                  # callback
        ctypes.c_void_p,                  # callback_data
    ]

    # opj_jp3d_transcode_to_ht
    lib.opj_jp3d_transcode_to_ht.restype  = ctypes.c_int32
    lib.opj_jp3d_transcode_to_ht.argtypes = [
        ctypes.c_char_p,                  # src_data
        ctypes.c_size_t,                  # src_size
        ctypes.POINTER(OPJ_EncParams),    # enc_params
        ctypes.POINTER(ctypes.c_void_p),  # out_data
        ctypes.POINTER(ctypes.c_size_t),  # out_size
        MsgCallbackType,                  # callback
        ctypes.c_void_p,                  # callback_data
    ]


_proto_done = False


def get_bindings():
    """Return the library handle with prototypes attached (idempotent)."""
    global _proto_done
    lib = get_lib()
    if not _proto_done:
        _setup_prototypes(lib)
        _proto_done = True
    return lib
