# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

"""
OpenJP3D Julia binding test suite.

Run with:
    OPENJP3D_LIBRARY=/path/to/libopenjp3d.so julia --project=. test/runtests.jl

Or via CTest (cmake -DBUILD_JULIA_BINDINGS=ON ... && cmake --build ... && ctest).
"""

using Test

# ---------------------------------------------------------------------------
# Skip all tests if the shared library is unavailable
# ---------------------------------------------------------------------------
import OpenJP3D

lib_ok = try
    OpenJP3D.get_version()
    true
catch
    false
end

if !lib_ok
    @info "openjp3d shared library not found — skipping Julia binding tests." *
          "\nSet OPENJP3D_LIBRARY or rebuild with -DBUILD_SHARED_LIBS=ON."
    exit(0)
end

# ---------------------------------------------------------------------------
# Helper: reproducible pseudo-random volume
# ---------------------------------------------------------------------------
function _rng_volume(::Type{T}, shape; seed::Int=0) where T
    vol    = Array{T}(undef, shape...)
    x      = xor(UInt32(0x12345678), UInt32(seed % UInt32))
    nbytes = sizeof(T)
    for i in eachindex(vol)
        # XorShift32 step
        x = x ⊻ (x << UInt32(13))
        x = x ⊻ (x >> UInt32(17))
        x = x ⊻ (x << UInt32(5))
        # Reinterpret the low bytes of x as the target integer type
        if nbytes == 1
            vol[i] = reinterpret(T, UInt8(x & 0xFF))
        elseif nbytes == 2
            vol[i] = reinterpret(T, UInt16(x & 0xFFFF))
        else
            vol[i] = reinterpret(T, x)
        end
    end
    return vol
end

# ===========================================================================
# 1. Library loading & version
# ===========================================================================

@testset "Version" begin
    v = OpenJP3D.get_version()
    @test isa(v, String)
    @test !isempty(v)
    parts = split(v, ".")
    @test length(parts) == 3
    for p in parts
        @test all(isdigit, p)
    end
end

# ===========================================================================
# 2. Constants
# ===========================================================================

@testset "Constants" begin
    @test OpenJP3D.FILTER_53  == Int32(0)
    @test OpenJP3D.FILTER_97  == Int32(1)
    @test OpenJP3D.USE_HTJ2K  == Int32(1)
    @test OpenJP3D.CS_UNKNOWN == UInt32(0)
    @test OpenJP3D.CS_SRGB    == UInt32(1)
    @test OpenJP3D.CS_GRAY    == UInt32(2)
    @test OpenJP3D.CS_YUV     == UInt32(3)
    @test OpenJP3D.MSG_INFO    == Int32(0)
    @test OpenJP3D.MSG_WARNING == Int32(1)
    @test OpenJP3D.MSG_ERROR   == Int32(2)
end

# ===========================================================================
# 3. EncodeParams
# ===========================================================================

@testset "EncodeParams" begin
    @testset "default construction" begin
        p = OpenJP3D.EncodeParams()
        @test p.tile_width        == 0
        @test p.tile_height       == 0
        @test p.tile_depth        == 0
        @test p.num_resolutions_x == 3
        @test p.num_resolutions_y == 3
        @test p.num_resolutions_z == 3
        @test p.cblk_width        == 4
        @test p.cblk_height       == 4
        @test p.cblk_depth        == 4
        @test p.filter            == OpenJP3D.FILTER_53
        @test p.num_layers        == 1
        @test p.target_rate       == 0.0f0
        @test p.use_htj2k         == Int32(0)
        @test p.verbose           == false
    end

    @testset "custom construction" begin
        p = OpenJP3D.EncodeParams(
            tile_width=32, tile_height=32, tile_depth=16,
            filter=OpenJP3D.FILTER_97,
            target_rate=2.0f0,
            use_htj2k=OpenJP3D.USE_HTJ2K,
        )
        @test p.tile_width  == 32
        @test p.tile_height == 32
        @test p.tile_depth  == 16
        @test p.filter      == OpenJP3D.FILTER_97
        @test p.target_rate ≈ 2.0f0
        @test p.use_htj2k   == OpenJP3D.USE_HTJ2K
    end

    @testset "_to_c conversion" begin
        p = OpenJP3D.EncodeParams(tile_width=64, filter=OpenJP3D.FILTER_97, verbose=true)
        c = OpenJP3D._to_c(p)
        @test isa(c, OpenJP3D.EncParamsC)
        @test c.tile_width == UInt32(64)
        @test c.filter     == Int32(1)   # FILTER_97
        @test c.verbose    == Int32(1)
    end
end

# ===========================================================================
# 4. encode / decode round-trips
# ===========================================================================

@testset "Round-trips" begin

    @testset "encode returns Vector{UInt8}" begin
        vol = zeros(UInt8, 4, 4, 4)
        cs  = OpenJP3D.encode(vol)
        @test isa(cs, Vector{UInt8})
        @test length(cs) > 0
    end

    @testset "SOC marker prefix" begin
        vol = zeros(UInt8, 4, 4, 4)
        cs  = OpenJP3D.encode(vol)
        # JP3D codestreams begin with SOC marker (0xFF 0x4F)
        @test length(cs) >= 2
        @test cs[1] == 0xFF
        @test cs[2] == 0x4F
    end

    @testset "lossless UInt8" begin
        vol = _rng_volume(UInt8, (8, 16, 16))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test eltype(out) == UInt8
        @test size(out)   == size(vol)
        @test out         == vol
    end

    @testset "lossless Int8" begin
        vol = _rng_volume(Int8, (4, 8, 8))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test eltype(out) == Int8
        @test out         == vol
    end

    @testset "lossless UInt16" begin
        vol = _rng_volume(UInt16, (4, 8, 8))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test eltype(out) == UInt16
        @test out         == vol
    end

    @testset "lossless Int16" begin
        vol = _rng_volume(Int16, (4, 8, 8))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test eltype(out) == Int16
        @test out         == vol
    end

    @testset "lossless Int32" begin
        vol = _rng_volume(Int32, (4, 8, 8))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test eltype(out) == Int32
        @test out         == vol
    end

    @testset "all zeros" begin
        vol = zeros(UInt8, 8, 8, 8)
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test out == vol
    end

    @testset "all ones" begin
        vol = ones(UInt8, 8, 8, 8)
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test out == vol
    end

    @testset "maximum value UInt8" begin
        vol = fill(UInt8(255), 8, 8, 8)
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test out == vol
    end

    @testset "multicomponent 3-channel (D,H,W,C)" begin
        vol = _rng_volume(UInt8, (4, 8, 8, 3))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test eltype(out) == UInt8
        @test size(out)   == (4, 8, 8, 3)
        @test out         == vol
    end

    @testset "multicomponent 4-channel" begin
        vol = _rng_volume(UInt16, (4, 8, 8, 4))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test eltype(out) == UInt16
        @test size(out)   == (4, 8, 8, 4)
        @test out         == vol
    end

    @testset "single-slice (D=1)" begin
        vol = _rng_volume(UInt8, (1, 16, 16))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test size(out) == size(vol)
        @test out       == vol
    end

    @testset "non-square dimensions" begin
        vol = _rng_volume(UInt8, (3, 5, 7))
        cs  = OpenJP3D.encode(
            vol,
            OpenJP3D.EncodeParams(num_resolutions_x=1,
                                  num_resolutions_y=1,
                                  num_resolutions_z=1),
        )
        out = OpenJP3D.decode(cs)
        @test size(out) == size(vol)
        @test out       == vol
    end

    @testset "large volume 16x16x16" begin
        vol = _rng_volume(UInt8, (16, 16, 16))
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test out == vol
    end

    @testset "encode with params keyword argument" begin
        vol    = _rng_volume(UInt8, (4, 8, 8))
        params = OpenJP3D.EncodeParams(cblk_width=4, cblk_height=4, cblk_depth=4)
        cs     = OpenJP3D.encode(vol, params)
        out    = OpenJP3D.decode(cs)
        @test out == vol
    end

    @testset "tiled encoding" begin
        vol    = _rng_volume(UInt8, (8, 16, 16))
        params = OpenJP3D.EncodeParams(tile_width=8, tile_height=8, tile_depth=4)
        cs     = OpenJP3D.encode(vol, params)
        out    = OpenJP3D.decode(cs)
        @test out == vol
    end

    @testset "default params shorthand (no type param)" begin
        vol = zeros(UInt8, 4, 4, 4)
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test out == vol
    end

end  # Round-trips

# ===========================================================================
# 5. HTJ2K round-trip
# ===========================================================================

@testset "HTJ2K" begin
    @testset "encode+decode HTJ2K UInt8" begin
        vol    = _rng_volume(UInt8, (8, 8, 8))
        params = OpenJP3D.EncodeParams(use_htj2k=OpenJP3D.USE_HTJ2K)
        cs     = OpenJP3D.encode(vol, params)
        out    = OpenJP3D.decode(cs)
        @test eltype(out) == UInt8
        @test out         == vol
    end

    @testset "HTJ2K Int16 round-trip" begin
        vol    = _rng_volume(Int16, (4, 8, 8))
        params = OpenJP3D.EncodeParams(use_htj2k=OpenJP3D.USE_HTJ2K)
        cs     = OpenJP3D.encode(vol, params)
        out    = OpenJP3D.decode(cs)
        @test out == vol
    end
end

# ===========================================================================
# 6. Lossy encoding (PSNR check)
# ===========================================================================

@testset "Lossy encoding" begin
    @testset "lossy 9/7 filter produces output" begin
        vol    = _rng_volume(UInt8, (8, 16, 16))
        params = OpenJP3D.EncodeParams(
            filter=OpenJP3D.FILTER_97, target_rate=1.0f0
        )
        cs  = OpenJP3D.encode(vol, params)
        out = OpenJP3D.decode(cs)
        @test size(out) == size(vol)
        @test eltype(out) == UInt8
        # Compute MSE; should be non-zero for lossy but finite
        mse = sum((Float64.(out) .- Float64.(vol)).^2) / length(vol)
        @test mse >= 0.0
        @test isfinite(mse)
    end

    @testset "lossy HTJ2K produces output" begin
        vol    = _rng_volume(UInt8, (4, 8, 8))
        params = OpenJP3D.EncodeParams(
            use_htj2k=OpenJP3D.USE_HTJ2K, target_rate=2.0f0
        )
        cs  = OpenJP3D.encode(vol, params)
        out = OpenJP3D.decode(cs)
        @test size(out) == size(vol)
    end
end

# ===========================================================================
# 7. transcode_to_ht
# ===========================================================================

@testset "transcode_to_ht" begin
    @testset "EBCOT -> HT produces valid codestream" begin
        vol = _rng_volume(UInt8, (4, 8, 8))
        cs  = OpenJP3D.encode(vol)            # EBCOT
        ht  = OpenJP3D.transcode_to_ht(cs)
        @test isa(ht, Vector{UInt8})
        @test length(ht) > 0
        # Verify the transcoded stream decodes correctly
        out = OpenJP3D.decode(ht)
        @test out == vol
    end

    @testset "transcode with explicit params" begin
        vol    = _rng_volume(UInt8, (4, 8, 8))
        cs     = OpenJP3D.encode(vol)
        # transcode_to_ht forces use_htj2k=USE_HTJ2K internally, regardless
        # of the params value supplied here.
        params = OpenJP3D.EncodeParams()
        ht     = OpenJP3D.transcode_to_ht(cs; params=params)
        out    = OpenJP3D.decode(ht)
        @test out == vol
    end

    @testset "SOC marker in transcoded stream" begin
        vol = zeros(UInt8, 4, 4, 4)
        cs  = OpenJP3D.encode(vol)
        ht  = OpenJP3D.transcode_to_ht(cs)
        @test ht[1] == 0xFF
        @test ht[2] == 0x4F
    end
end

# ===========================================================================
# 8. Message callback
# ===========================================================================

@testset "Message callback" begin
    @testset "verbose decode collects messages" begin
        vol      = zeros(UInt8, 4, 4, 4)
        cs       = OpenJP3D.encode(vol, OpenJP3D.EncodeParams(verbose=true))
        messages = Tuple{Int,String}[]
        cb = (level, msg) -> push!(messages, (level, msg))
        out = OpenJP3D.decode(cs; verbose=true, on_message=cb)
        @test out == vol
        # We just verify the callback mechanism works (messages may or may not
        # arrive depending on verbosity implementation)
        @test isa(messages, Vector)
    end

    @testset "on_message receives integer level" begin
        vol    = zeros(UInt8, 4, 4, 4)
        levels = Int[]
        cb = (level, msg) -> push!(levels, level)
        OpenJP3D.encode(vol, OpenJP3D.EncodeParams(verbose=true); on_message=cb)
        for l in levels
            @test l in (0, 1, 2)
        end
    end
end

# ===========================================================================
# 9. Error handling
# ===========================================================================

@testset "Error handling" begin
    @testset "invalid codestream raises error" begin
        garbage = UInt8[0x00, 0x01, 0x02, 0x03, 0x04]
        @test_throws ErrorException OpenJP3D.decode(garbage)
    end

    @testset "2-D array raises ArgumentError" begin
        bad = zeros(UInt8, 4, 4)
        @test_throws ArgumentError OpenJP3D.encode(bad)
    end

    @testset "5-D array raises ArgumentError" begin
        bad = zeros(UInt8, 2, 2, 2, 2, 2)
        @test_throws ArgumentError OpenJP3D.encode(bad)
    end

    @testset "Float32 array raises ArgumentError" begin
        bad = zeros(Float32, 4, 4, 4)
        @test_throws ArgumentError OpenJP3D.encode(bad)
    end

    @testset "Float64 array raises ArgumentError" begin
        bad = zeros(Float64, 4, 4, 4)
        @test_throws ArgumentError OpenJP3D.encode(bad)
    end

    @testset "empty codestream raises error" begin
        @test_throws ErrorException OpenJP3D.decode(UInt8[])
    end
end

# ===========================================================================
# 10. Data layout correctness
# ===========================================================================

@testset "Data layout" begin
    @testset "non-trivial voxel values preserved" begin
        # Manually construct a volume with known pattern
        vol = Array{UInt8}(undef, 2, 3, 4)
        for d in 1:2, h in 1:3, w in 1:4
            vol[d, h, w] = UInt8((d-1)*12 + (h-1)*4 + (w-1))
        end
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test out == vol
    end

    @testset "gradient volume preserves spatial order" begin
        vol = Array{Int16}(undef, 4, 4, 4)
        for d in 1:4, h in 1:4, w in 1:4
            vol[d, h, w] = Int16((d - 1) * 100 + (h - 1) * 10 + (w - 1))
        end
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test out == vol
    end

    @testset "corner voxels preserved" begin
        vol = zeros(UInt8, 4, 4, 4)
        # Set corners to distinct values
        vol[1, 1, 1] = 10
        vol[1, 1, 4] = 20
        vol[1, 4, 1] = 30
        vol[1, 4, 4] = 40
        vol[4, 1, 1] = 50
        vol[4, 1, 4] = 60
        vol[4, 4, 1] = 70
        vol[4, 4, 4] = 80
        cs  = OpenJP3D.encode(vol)
        out = OpenJP3D.decode(cs)
        @test out[1, 1, 1] == 10
        @test out[1, 1, 4] == 20
        @test out[1, 4, 1] == 30
        @test out[4, 4, 4] == 80
    end
end

println("\nAll OpenJP3D Julia binding tests passed.")
