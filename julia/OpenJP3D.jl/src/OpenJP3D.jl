# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

"""
    OpenJP3D

Julia bindings for the OpenJP3D volumetric codec library.

Provides `encode`, `decode`, `transcode_to_ht`, `get_version`, and
`EncodeParams` matching the Python `openjp3d` package API.

# Quick start

```julia
using OpenJP3D

# Create a synthetic 3-D volume (D, H, W)
vol = rand(UInt8, 16, 32, 32)

# Encode to a JP3D codestream (Vector{UInt8})
cs = encode(vol)

# Decode back to an Array
decoded = decode(cs)
@assert decoded == vol  # lossless by default
```
"""
module OpenJP3D

using Libdl

# ---------------------------------------------------------------------------
# Library loading
# ---------------------------------------------------------------------------

const _LIB_BASENAME = Sys.iswindows() ? "openjp3d" : "libopenjp3d"
const _lib_path_cache   = Ref{String}("")
const _lib_handle_cache = Ref{Ptr{Cvoid}}(C_NULL)

"""
    lib_path() -> String

Return the path (or bare name) of the openjp3d shared library.

Search order:
1. `OPENJP3D_LIBRARY` environment variable.
2. A `lib/` subdirectory relative to the package source directory.
3. `Libdl.find_library("libopenjp3d")` (searches `LD_LIBRARY_PATH` / `PATH`).
4. Bare library name (let the OS linker search at `dlopen` time).
"""
function lib_path()::String
    isempty(_lib_path_cache[]) || return _lib_path_cache[]

    # 1) Environment variable
    env = get(ENV, "OPENJP3D_LIBRARY", "")
    if !isempty(env)
        _lib_path_cache[] = env
        return _lib_path_cache[]
    end

    # 2) lib/ next to the package directory
    exts = Sys.iswindows() ? (".dll",) :
           Sys.isapple()   ? (".dylib", ".so") :
                             (".so", ".dylib")
    for ext in exts
        candidate = normpath(joinpath(@__DIR__, "..", "lib", _LIB_BASENAME * ext))
        if isfile(candidate)
            _lib_path_cache[] = candidate
            return _lib_path_cache[]
        end
    end

    # 3) System search
    found = Libdl.find_library(_LIB_BASENAME)
    if !isempty(found)
        _lib_path_cache[] = found
        return _lib_path_cache[]
    end

    # 4) Bare name
    _lib_path_cache[] = _LIB_BASENAME
    return _lib_path_cache[]
end

"""Return a cached `dlopen` handle to the openjp3d shared library."""
function _lib_handle()::Ptr{Cvoid}
    if _lib_handle_cache[] == C_NULL
        _lib_handle_cache[] = Libdl.dlopen(lib_path())
    end
    return _lib_handle_cache[]
end

"""Resolve a symbol in the openjp3d shared library (cached handle)."""
function _sym(name::Symbol)
    return Libdl.dlsym(_lib_handle(), name)
end

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

"Lossless 5/3 integer lifting filter (default)."
const FILTER_53  = Int32(0)

"Lossy 9/7 floating-point lifting filter."
const FILTER_97  = Int32(1)

"Flag to enable the HTJ2K high-throughput block coder."
const USE_HTJ2K  = Int32(1)

"Unknown colour space."
const CS_UNKNOWN = UInt32(0)

"sRGB colour space."
const CS_SRGB    = UInt32(1)

"Greyscale colour space."
const CS_GRAY    = UInt32(2)

"YCbCr (YUV) colour space."
const CS_YUV     = UInt32(3)

"Message severity: informational."
const MSG_INFO    = Int32(0)

"Message severity: warning."
const MSG_WARNING = Int32(1)

"Message severity: error."
const MSG_ERROR   = Int32(2)

# ---------------------------------------------------------------------------
# C structure mirrors (layout must match openjp3d.h exactly)
# ---------------------------------------------------------------------------

"""Mirror of `opj_volume_comp_t`."""
struct VolumeComp
    w::UInt32
    h::UInt32
    d::UInt32
    prec::UInt32
    sgnd::Int32
    dz::Float32
    data::Ptr{Int32}
end

"""Mirror of `opj_volume_t`."""
struct Volume
    numcomps::UInt32
    comps::Ptr{VolumeComp}
    x0::UInt32
    y0::UInt32
    z0::UInt32
    x1::UInt32
    y1::UInt32
    z1::UInt32
    color_space::UInt32
end

"""Mutable mirror of `opj_jp3d_enc_params_t` (for passing to C by reference)."""
mutable struct EncParamsC
    tile_width::UInt32
    tile_height::UInt32
    tile_depth::UInt32
    num_resolutions_x::UInt32
    num_resolutions_y::UInt32
    num_resolutions_z::UInt32
    cblk_width::UInt32
    cblk_height::UInt32
    cblk_depth::UInt32
    filter::Int32
    num_layers::UInt32
    target_rate::Float32
    use_htj2k::Int32
    verbose::Int32
end

"""Mutable mirror of `opj_jp3d_dec_params_t`."""
mutable struct DecParamsC
    verbose::Int32
end

# ---------------------------------------------------------------------------
# Encoder parameter struct
# ---------------------------------------------------------------------------

"""
    EncodeParams(; kwargs...)

Encoder parameters for [`encode`](@ref).  All fields mirror
`opj_jp3d_enc_params_t` with Julia-friendly defaults.

| Field               | Type    | Default       | Description                                   |
|---------------------|---------|---------------|-----------------------------------------------|
| `tile_width`        | `Int`   | `0`           | Tile width (0 = whole volume)                 |
| `tile_height`       | `Int`   | `0`           | Tile height (0 = whole volume)                |
| `tile_depth`        | `Int`   | `0`           | Tile depth (0 = whole volume)                 |
| `num_resolutions_x` | `Int`   | `3`           | DWT decomposition levels along X              |
| `num_resolutions_y` | `Int`   | `3`           | DWT decomposition levels along Y              |
| `num_resolutions_z` | `Int`   | `3`           | DWT decomposition levels along Z              |
| `cblk_width`        | `Int`   | `4`           | Code-block width                              |
| `cblk_height`       | `Int`   | `4`           | Code-block height                             |
| `cblk_depth`        | `Int`   | `4`           | Code-block depth                              |
| `filter`            | `Int32` | `FILTER_53`   | `FILTER_53` (lossless) or `FILTER_97` (lossy) |
| `num_layers`        | `Int`   | `1`           | Number of quality layers                      |
| `target_rate`       | `Float32` | `0.0f0`     | Target bits/sample (0 = lossless)             |
| `use_htj2k`         | `Int32` | `Int32(0)`    | Set to `USE_HTJ2K` to enable HT block coder   |
| `verbose`           | `Bool`  | `false`       | Enable codec info messages                    |
"""
Base.@kwdef struct EncodeParams
    tile_width::Int        = 0
    tile_height::Int       = 0
    tile_depth::Int        = 0
    num_resolutions_x::Int = 3
    num_resolutions_y::Int = 3
    num_resolutions_z::Int = 3
    cblk_width::Int        = 4
    cblk_height::Int       = 4
    cblk_depth::Int        = 4
    filter::Int32          = FILTER_53
    num_layers::Int        = 1
    target_rate::Float32   = 0.0f0
    use_htj2k::Int32       = Int32(0)
    verbose::Bool          = false
end

"""Convert `EncodeParams` to a C-level `EncParamsC` struct."""
function _to_c(p::EncodeParams)::EncParamsC
    EncParamsC(
        UInt32(p.tile_width),
        UInt32(p.tile_height),
        UInt32(p.tile_depth),
        UInt32(p.num_resolutions_x),
        UInt32(p.num_resolutions_y),
        UInt32(p.num_resolutions_z),
        UInt32(p.cblk_width),
        UInt32(p.cblk_height),
        UInt32(p.cblk_depth),
        Int32(p.filter),
        UInt32(p.num_layers),
        Float32(p.target_rate),
        Int32(p.use_htj2k),
        Int32(p.verbose),
    )
end

# ---------------------------------------------------------------------------
# Callback support
#
# Julia's ccall with a tuple (name, lib) requires lib to be a constant, so
# we use Libdl.dlsym to obtain function pointers at runtime.  For the message
# callback, we use a module-level @cfunction with a global slot so the C
# function pointer is stable (no heap allocation per call).
# Note: the single global slot is not thread-safe for concurrent calls.
# ---------------------------------------------------------------------------

const _msg_slot = Ref{Any}(nothing)

function _global_cb(level::Cint, msg::Cstring, _::Ptr{Cvoid})::Cvoid
    cb = _msg_slot[]
    cb === nothing && return nothing
    text = msg == C_NULL ? "" : unsafe_string(msg)
    cb(Int(level), text)
    return nothing
end

const _C_CALLBACK = @cfunction(_global_cb, Cvoid, (Cint, Cstring, Ptr{Cvoid}))

"""
Execute `f(cb_ptr)` with the module-level callback set to `on_message`
(or `C_NULL` when `on_message` is `nothing`).
"""
function _with_cb(f::Function, on_message)
    if on_message === nothing
        return f(C_NULL)
    else
        _msg_slot[] = on_message
        try
            return f(_C_CALLBACK)
        finally
            _msg_slot[] = nothing
        end
    end
end

# ---------------------------------------------------------------------------
# Data layout helpers
#
# Julia arrays are column-major (first index varies fastest).
# The C codec stores data[z*w*h + y*w + x] — row-major with x fastest.
# For a (D,H,W) array:
#   • encode: permutedims(arr, (3,2,1)) -> (W,H,D); vec gives w-major order
#   • decode: reshape(raw, w, h, d) -> permutedims(..., (3,2,1)) -> (D,H,W)
# ---------------------------------------------------------------------------

"""Convert (D,H,W) Julia array slice to a flat Int32 vector in C row-major order."""
function _to_row_major(slice::AbstractArray{T,3})::Vector{Int32} where T
    return vec(permutedims(Array{Int32}(slice), (3, 2, 1)))
end

"""Reshape a flat Int32 C-order buffer into a (D,H,W) Julia array."""
function _from_row_major(raw::Vector{Int32}, w::Int, h::Int, d::Int, ::Type{T}) where T
    out = permutedims(reshape(raw, w, h, d), (3, 2, 1))
    # Clamp to the valid range of T before conversion (guards against lossy
    # IDWT rounding producing values outside the encoded type's range).
    lo = Int32(typemin(T))
    hi = Int32(typemax(T))
    return T.(clamp.(out, lo, hi))
end

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

export get_version, encode, decode, transcode_to_ht
export EncodeParams
export FILTER_53, FILTER_97, USE_HTJ2K
export CS_UNKNOWN, CS_SRGB, CS_GRAY, CS_YUV
export MSG_INFO, MSG_WARNING, MSG_ERROR

"""
    get_version() -> String

Return the native OpenJP3D library version string (e.g. `"1.0.0"`).
"""
function get_version()::String
    ptr = ccall(_sym(:opj_jp3d_get_version), Cstring, ())
    ptr == C_NULL && error("opj_jp3d_get_version() returned NULL")
    return unsafe_string(ptr)
end

"""
    encode(volume [, params]; on_message=nothing) -> Vector{UInt8}

Encode a 3-D volume to a JP3D codestream.

# Arguments

- `volume`: An `AbstractArray` of shape `(D, H, W)` or `(D, H, W, C)`.
  Supported element types: `UInt8`, `Int8`, `UInt16`, `Int16`, `Int32`.
- `params`: [`EncodeParams`](@ref) (optional; uses lossless defaults when
  omitted).
- `on_message`: Optional callback `(level::Int, msg::String) -> nothing`
  that receives codec messages.  `level` is `MSG_INFO`, `MSG_WARNING`, or
  `MSG_ERROR`.

# Returns

A `Vector{UInt8}` containing the raw JP3D codestream.

# Throws

- `ArgumentError` if the array has unsupported dimensions or element type.
- `ErrorException` if the codec fails.

!!! warning "Thread safety"
    The `on_message` callback uses a module-level global slot.
    Concurrent calls to `encode`, `decode`, or `transcode_to_ht` with
    non-`nothing` `on_message` arguments from different threads may
    interleave their callbacks.  For thread-safe usage, set `on_message`
    to `nothing` and use `verbose=true` for diagnostics.
"""
function encode(
    volume::AbstractArray{T},
    params::EncodeParams = EncodeParams();
    on_message = nothing,
)::Vector{UInt8} where {T}

    nd = ndims(volume)
    if nd == 3
        d, h, w   = size(volume)
        numcomps  = 1
        arr = reshape(volume, d, h, w, 1)
    elseif nd == 4
        d, h, w, numcomps = size(volume)
        arr = volume
    else
        throw(ArgumentError(
            "Expected 3-D (D,H,W) or 4-D (D,H,W,C) array, got $(nd)-D"
        ))
    end

    (prec, sgnd) =
        T == UInt8  ? (UInt32(8),  Int32(0)) :
        T == Int8   ? (UInt32(8),  Int32(1)) :
        T == UInt16 ? (UInt32(16), Int32(0)) :
        T == Int16  ? (UInt32(16), Int32(1)) :
        T == Int32  ? (UInt32(32), Int32(1)) :
        throw(ArgumentError(
            "Unsupported element type $T. " *
            "Supported: UInt8, Int8, UInt16, Int16, Int32."
        ))

    vol_ptr = ccall(
        _sym(:opj_jp3d_create_volume), Ptr{Volume},
        (UInt32, UInt32, UInt32, UInt32, UInt32, Int32),
        UInt32(numcomps), UInt32(w), UInt32(h), UInt32(d), prec, sgnd,
    )
    vol_ptr == C_NULL && error("opj_jp3d_create_volume() returned NULL")

    try
        vol      = unsafe_load(vol_ptr)
        n_voxels = Int(w) * Int(h) * Int(d)

        for c in 1:numcomps
            comp = unsafe_load(vol.comps, c)
            flat = _to_row_major(arr[:, :, :, c])
            GC.@preserve flat begin
                unsafe_copyto!(comp.data, pointer(flat), n_voxels)
            end
        end

        c_params = _to_c(params)
        out_ptr  = Ref{Ptr{UInt8}}(C_NULL)
        out_size = Ref{Csize_t}(0)

        ok = _with_cb(on_message) do cb_ptr
            ccall(
                _sym(:opj_jp3d_encode), Int32,
                (Ptr{Volume}, Ref{EncParamsC}, Ref{Ptr{UInt8}}, Ref{Csize_t},
                 Ptr{Cvoid}, Ptr{Cvoid}),
                vol_ptr, Ref(c_params), out_ptr, out_size, cb_ptr, C_NULL,
            )
        end
        ok != Int32(1) && error("opj_jp3d_encode() failed")

        n      = Int(out_size[])
        result = Vector{UInt8}(undef, n)
        GC.@preserve result begin
            unsafe_copyto!(pointer(result), out_ptr[], n)
        end
        ccall(_sym(:opj_jp3d_free), Cvoid, (Ptr{Cvoid},), out_ptr[])
        return result

    finally
        ccall(_sym(:opj_jp3d_destroy_volume), Cvoid, (Ptr{Volume},), vol_ptr)
    end
end

"""
    decode(data; verbose=false, on_message=nothing) -> Array

Decode a JP3D codestream to a Julia array.

# Arguments

- `data`: A `Vector{UInt8}` containing the raw JP3D codestream.
- `verbose`: Enable codec info messages (default `false`).
- `on_message`: Optional callback `(level::Int, msg::String) -> nothing`.

# Returns

An `Array` of shape `(D, H, W)` for single-component volumes or
`(D, H, W, C)` for multi-component volumes.
The element type is `UInt8`, `Int8`, `UInt16`, `Int16`, or `Int32`
according to the encoded bit-depth and signedness.

# Throws

- `ErrorException` if decoding fails.
"""
function decode(
    data::AbstractVector{UInt8};
    verbose::Bool = false,
    on_message = nothing,
)
    dec_params = DecParamsC(Int32(verbose))

    vol_ptr = _with_cb(on_message) do cb_ptr
        ccall(
            _sym(:opj_jp3d_decode), Ptr{Volume},
            (Ptr{UInt8}, Csize_t, Ref{DecParamsC}, Ptr{Cvoid}, Ptr{Cvoid}),
            pointer(data), Csize_t(length(data)),
            Ref(dec_params), cb_ptr, C_NULL,
        )
    end
    vol_ptr == C_NULL && error("opj_jp3d_decode() returned NULL — decoding failed")

    try
        vol      = unsafe_load(vol_ptr)
        numcomps = Int(vol.numcomps)
        comp0    = unsafe_load(vol.comps, 1)
        w, h, d  = Int(comp0.w), Int(comp0.h), Int(comp0.d)
        prec     = Int(comp0.prec)
        sgnd     = Int(comp0.sgnd)
        n_voxels = w * h * d

        OutT =
            prec <= 8  ? (sgnd != 0 ? Int8  : UInt8)  :
            prec <= 16 ? (sgnd != 0 ? Int16 : UInt16) :
                          Int32

        if numcomps == 1
            comp = unsafe_load(vol.comps, 1)
            raw  = Vector{Int32}(undef, n_voxels)
            GC.@preserve raw begin
                unsafe_copyto!(pointer(raw), comp.data, n_voxels)
            end
            return _from_row_major(raw, w, h, d, OutT)
        else
            result = Array{OutT}(undef, d, h, w, numcomps)
            for c in 1:numcomps
                comp = unsafe_load(vol.comps, c)
                raw  = Vector{Int32}(undef, n_voxels)
                GC.@preserve raw begin
                    unsafe_copyto!(pointer(raw), comp.data, n_voxels)
                end
                result[:, :, :, c] = _from_row_major(raw, w, h, d, OutT)
            end
            return result
        end

    finally
        ccall(_sym(:opj_jp3d_destroy_volume), Cvoid, (Ptr{Volume},), vol_ptr)
    end
end

"""
    transcode_to_ht(data; params=nothing, on_message=nothing) -> Vector{UInt8}

Losslessly transcode a JP3D codestream from EBCOT to HTJ2K block coding.

# Arguments

- `data`: Source JP3D codestream (`Vector{UInt8}`).
- `params`: Optional [`EncodeParams`](@ref).  The `use_htj2k` field is forced
  to `USE_HTJ2K` regardless of the supplied value.  Pass `nothing` (default)
  to derive encoder parameters from the source codestream header.
- `on_message`: Optional message callback `(level::Int, msg::String) -> nothing`.

# Returns

A `Vector{UInt8}` containing the transcoded codestream using the HT block
coder.

# Throws

- `ErrorException` if transcoding fails.
"""
function transcode_to_ht(
    data::AbstractVector{UInt8};
    params::Union{EncodeParams,Nothing} = nothing,
    on_message = nothing,
)::Vector{UInt8}
    out_ptr  = Ref{Ptr{UInt8}}(C_NULL)
    out_size = Ref{Csize_t}(0)

    if params === nothing
        ok = _with_cb(on_message) do cb_ptr
            ccall(
                _sym(:opj_jp3d_transcode_to_ht), Int32,
                (Ptr{UInt8}, Csize_t, Ptr{Cvoid}, Ref{Ptr{UInt8}}, Ref{Csize_t},
                 Ptr{Cvoid}, Ptr{Cvoid}),
                pointer(data), Csize_t(length(data)),
                C_NULL, out_ptr, out_size, cb_ptr, C_NULL,
            )
        end
    else
        c_params           = _to_c(params)
        c_params.use_htj2k = USE_HTJ2K
        ok = _with_cb(on_message) do cb_ptr
            ccall(
                _sym(:opj_jp3d_transcode_to_ht), Int32,
                (Ptr{UInt8}, Csize_t, Ref{EncParamsC}, Ref{Ptr{UInt8}}, Ref{Csize_t},
                 Ptr{Cvoid}, Ptr{Cvoid}),
                pointer(data), Csize_t(length(data)),
                Ref(c_params), out_ptr, out_size, cb_ptr, C_NULL,
            )
        end
    end

    ok != Int32(1) && error("opj_jp3d_transcode_to_ht() failed")

    n      = Int(out_size[])
    result = Vector{UInt8}(undef, n)
    GC.@preserve result begin
        unsafe_copyto!(pointer(result), out_ptr[], n)
    end
    ccall(_sym(:opj_jp3d_free), Cvoid, (Ptr{Cvoid},), out_ptr[])
    return result
end

end # module OpenJP3D
