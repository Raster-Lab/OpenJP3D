% Copyright (c) 2024-2026, OpenJP3D Contributors
% All rights reserved.
% SPDX-License-Identifier: BSD-2-Clause

% OpenJP3D MATLAB/Octave binding test suite
%
% Run with:
%   OPENJP3D_LIBRARY=/path/to/libopenjp3d.so \
%       octave --no-gui --path matlab/openjp3d --path /build tests/test_matlab.m
%
% Or via CTest after configuring with -DBUILD_MATLAB_BINDINGS=ON.

% Add package path if not already on the search path
this_dir = fileparts(mfilename('fullpath'));
pkg_dir  = fullfile(fileparts(this_dir), 'matlab', 'openjp3d');
if exist(pkg_dir, 'dir')
    addpath(pkg_dir);
end

% ---------------------------------------------------------------------------
% Try to load the library; skip gracefully if unavailable
% ---------------------------------------------------------------------------

lib_ok = false;
try
    openjp3d.get_version();
    lib_ok = true;
catch
    % ignore — will skip below
end

if ~lib_ok
    fprintf(['openjp3d shared library not available — skipping ' ...
             'MATLAB/Octave binding tests.\n' ...
             'Set OPENJP3D_LIBRARY or rebuild with -DBUILD_SHARED_LIBS=ON.\n']);
    return;
end

% ---------------------------------------------------------------------------
% Test counters
% ---------------------------------------------------------------------------
n_pass = 0;
n_fail = 0;

% ---------------------------------------------------------------------------
% 1. Library loading & version
% ---------------------------------------------------------------------------

t = test_run('get_version returns a non-empty string', ...
    @() test_assert_true(~isempty(openjp3d.get_version())));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('version has MAJOR.MINOR.PATCH format', ...
    @() test_assert_true(numel(strsplit(openjp3d.get_version(), '.')) == 3));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 2. EncodeParams construction
% ---------------------------------------------------------------------------

t = test_run('EncodeParams default Filter == 0 (FILTER_53)', ...
    @() test_assert_equal(EncodeParams().Filter, int32(0)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams default NumResolutionsX == 3', ...
    @() test_assert_equal(EncodeParams().NumResolutionsX, int32(3)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams default TileWidth == 0', ...
    @() test_assert_equal(EncodeParams().TileWidth, int32(0)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams default TargetRate == 0', ...
    @() test_assert_true(abs(EncodeParams().TargetRate) < 1e-9));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams default UseHTJ2K == 0', ...
    @() test_assert_equal(EncodeParams().UseHTJ2K, int32(0)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams Filter=1 (FILTER_97) accepted', ...
    @() test_assert_equal(EncodeParams('Filter', 1).Filter, int32(1)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams TileWidth set', ...
    @() test_assert_equal(EncodeParams('TileWidth', 32).TileWidth, int32(32)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams TargetRate set', ...
    @() test_assert_true(abs(EncodeParams('TargetRate',1.5).TargetRate-1.5)<1e-9));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams UseHTJ2K set', ...
    @() test_assert_equal(EncodeParams('UseHTJ2K', 1).UseHTJ2K, int32(1)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams Verbose flag coerced to int32(1)', ...
    @() test_assert_equal(EncodeParams('Verbose', true).Verbose, int32(1)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams to_c produces int32(13)', ...
    @() test_assert_true( ...
        numel(EncodeParams().to_c().params_int)==13 && ...
        isa(EncodeParams().to_c().params_int,'int32')));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams to_c target_rate is double', ...
    @() test_assert_true(isa(EncodeParams().to_c().target_rate,'double')));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('EncodeParams unknown property errors', ...
    @() test_assert_error(@() EncodeParams('Bogus',0)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 3. Lossless round-trips — all supported precisions
% ---------------------------------------------------------------------------

t = test_run('lossless round-trip: 8-bit unsigned (uint8)', ...
    @() do_roundtrip([4 8 8], 1, 8, false));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('lossless round-trip: 8-bit signed (int8)', ...
    @() do_roundtrip([4 8 8], 2, 8, true));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('lossless round-trip: 16-bit unsigned (uint16)', ...
    @() do_roundtrip([4 8 8], 3, 16, false));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('lossless round-trip: 16-bit signed (int16)', ...
    @() do_roundtrip([4 8 8], 4, 16, true));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('lossless round-trip: 32-bit signed (int32)', ...
    @() do_roundtrip([4 8 8], 5, 32, true));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 4. Multi-component volumes
% ---------------------------------------------------------------------------

t = test_run('lossless round-trip: 3-component volume', ...
    @() do_roundtrip([4 8 8 3], 10, 8, false));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('lossless round-trip: 4-component volume', ...
    @() do_roundtrip([4 8 8 4], 11, 8, false));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 5. Single-slice edge case (D=1)
% ---------------------------------------------------------------------------

t = test_run('single-slice volume (D=1) round-trips losslessly', ...
    @() do_roundtrip([1 8 8], 20, 8, false));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 6. Non-square dimensions
% ---------------------------------------------------------------------------

t = test_run('non-square volume [3 5 7] round-trips losslessly', ...
    @() do_roundtrip([3 5 7], 30, 8, false));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 7. Larger volume (16x16x16)
% ---------------------------------------------------------------------------

t = test_run('16x16x16 uint8 volume round-trips losslessly', ...
    @() do_roundtrip([16 16 16], 40, 8, false));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 8. Tiled encoding
% ---------------------------------------------------------------------------

t = test_run('tiled encoding [8 16 16] round-trips losslessly', ...
    @() do_roundtrip_params([8 16 16], 50, 8, false, ...
        EncodeParams('TileWidth',8,'TileHeight',8,'TileDepth',4)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 9. SOC marker check
% ---------------------------------------------------------------------------

t = test_run('codestream starts with JP3D SOC marker 0xFF 0x4F', ...
    @() do_soc_marker_test());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 10. HTJ2K round-trip
% ---------------------------------------------------------------------------

t = test_run('HTJ2K round-trip is lossless (5/3 filter + UseHTJ2K)', ...
    @() do_roundtrip_params([4 8 8], 70, 8, false, EncodeParams('UseHTJ2K',1)));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 11. Lossy encoding (9/7 filter)
% ---------------------------------------------------------------------------

t = test_run('lossy 9/7 encoding produces a valid decodable codestream', ...
    @() do_lossy_test());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 12. transcode_to_ht
% ---------------------------------------------------------------------------

t = test_run('transcode_to_ht (no params) produces decodable codestream', ...
    @() do_transcode_no_params());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('transcode_to_ht (with params) produces decodable codestream', ...
    @() do_transcode_with_params());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 13. Data layout correctness — corner voxels
% ---------------------------------------------------------------------------

t = test_run('corner voxels are preserved after encode->decode', ...
    @() do_corner_voxel_test());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('gradient pattern is preserved (data layout correctness)', ...
    @() do_gradient_layout_test());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 14. Message callback
% ---------------------------------------------------------------------------

t = test_run('message callback can be registered without error', ...
    @() do_callback_test());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 15. decode() dtype output
% ---------------------------------------------------------------------------

t = test_run('decoded uint8 volume returns dtype ''uint8''', ...
    @() do_dtype_test(8, false, 'uint8'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('decoded int8 volume returns dtype ''int8''', ...
    @() do_dtype_test(8, true, 'int8'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('decoded uint16 volume returns dtype ''uint16''', ...
    @() do_dtype_test(16, false, 'uint16'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('decoded int16 volume returns dtype ''int16''', ...
    @() do_dtype_test(16, true, 'int16'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('decoded int32 volume returns dtype ''int32''', ...
    @() do_dtype_test(32, true, 'int32'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 16. Class-inferred prec/sgnd
% ---------------------------------------------------------------------------

t = test_run('int8 input infers prec=8/sgnd (dtype=int8 round-trips)', ...
    @() do_infer_class_test(int8(rng_volume([4 8 8],200,8,true)), 'int8'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('uint8 input infers prec=8/unsigned (dtype=uint8 round-trips)', ...
    @() do_infer_class_test(uint8(rng_volume([4 8 8],201,8,false)), 'uint8'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('int16 input infers prec=16/sgnd (dtype=int16 round-trips)', ...
    @() do_infer_class_test(int16(rng_volume([4 8 8],202,16,true)), 'int16'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('uint16 input infers prec=16/unsigned (dtype=uint16 round-trips)', ...
    @() do_infer_class_test(uint16(rng_volume([4 8 8],203,16,false)), 'uint16'));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 17. Error handling
% ---------------------------------------------------------------------------

t = test_run('encode() rejects non-numeric input', ...
    @() test_assert_error(@() openjp3d.encode('hello')));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('encode() rejects 2-D input', ...
    @() test_assert_error(@() openjp3d.encode(int32(zeros(4,8)))));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('decode() rejects non-uint8 input', ...
    @() test_assert_error(@() openjp3d.decode(int32([255 79]))));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('decode() fails on invalid codestream', ...
    @() test_assert_error(@() openjp3d.decode(uint8([0 1 2 3]))));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

t = test_run('transcode_to_ht() rejects non-uint8 input', ...
    @() test_assert_error(@() openjp3d.transcode_to_ht(int32(1:10))));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 18. Verbose decode
% ---------------------------------------------------------------------------

t = test_run('decode with Verbose=true does not crash', ...
    @() do_verbose_decode_test());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 19. OnMessage callback for decode
% ---------------------------------------------------------------------------

t = test_run('decode OnMessage callback accepted without error', ...
    @() do_decode_callback_test());
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% 20. Large 32-bit signed volume
% ---------------------------------------------------------------------------

t = test_run('32-bit signed 8x8x8 volume round-trips losslessly', ...
    @() do_roundtrip([8 8 8], 300, 32, true));
if t; n_pass=n_pass+1; else; n_fail=n_fail+1; end

% ---------------------------------------------------------------------------
% Summary
% ---------------------------------------------------------------------------

fprintf('\nResult: %d passed, %d failed\n', n_pass, n_fail);
if n_fail > 0
    error('openjp3d:test_failed', '%d test(s) failed', n_fail);
end

% ===========================================================================
% Helper functions (must appear after the main script body)
% ===========================================================================

function result = test_run(desc, fn)
% Execute a zero-argument function handle and report pass/fail.
    try
        fn();
        fprintf('pass: %s\n', desc);
        result = true;
    catch e
        fprintf('FAIL: %s\n  Error: %s\n', desc, e.message);
        result = false;
    end
end

function test_assert_true(x, msg)
    if nargin < 2; msg = ''; end
    if ~x
        if isempty(msg)
            error('Expected TRUE, got FALSE');
        else
            error('%s', msg);
        end
    end
end

function test_assert_equal(a, b)
    if ~isequal(a, b)
        n = min(3, max(numel(a), numel(b)));
        error('Values differ.\n  got:      %s\n  expected: %s', ...
              mat2str(a(1:min(n,numel(a)))), mat2str(b(1:min(n,numel(b)))));
    end
end

function test_assert_error(fn, pattern)
    if nargin < 2; pattern = ''; end
    caught = false;
    try
        fn();
    catch e
        if isempty(pattern)
            caught = true;
        else
            caught = ~isempty(regexpi(e.message, pattern));
        end
    end
    if ~caught
        if isempty(pattern)
            error('Expected an error but none was thrown');
        else
            error('Expected error matching ''%s'' but none was thrown', pattern);
        end
    end
end

function vol = rng_volume(shape, seed, prec, sgnd)
% Create a reproducible pseudo-random int32 volume.
    if nargin < 3; prec = 32; end
    if nargin < 4; sgnd = true; end
    rng(seed);
    n = prod(shape);
    if prec == 8
        lo = 0; hi = 255; if sgnd; lo = -128; hi = 127; end
    elseif prec == 16
        lo = 0; hi = 65535; if sgnd; lo = -32768; hi = 32767; end
    else
        lo = -2147483647; hi = 2147483647;
    end
    vals = int32(randi([lo hi], 1, n));
    vol  = reshape(vals, shape);
end

function do_roundtrip(shape, seed, prec, sgnd)
    vol  = rng_volume(shape, seed, prec, sgnd);
    opts.Prec = prec; opts.Sgnd = sgnd;
    cs   = openjp3d.encode(vol, EncodeParams(), opts);
    test_assert_true(isa(cs, 'uint8'), 'encode did not return uint8');
    [out, ~] = openjp3d.decode(cs);
    test_assert_equal(out, int32(vol));
end

function do_roundtrip_params(shape, seed, prec, sgnd, params)
    vol  = rng_volume(shape, seed, prec, sgnd);
    opts.Prec = prec; opts.Sgnd = sgnd;
    cs   = openjp3d.encode(vol, params, opts);
    [out, ~] = openjp3d.decode(cs);
    test_assert_equal(out, int32(vol));
end

function do_soc_marker_test()
    vol  = rng_volume([4 8 8], 60, 8, false);
    opts.Prec = 8; opts.Sgnd = false;
    cs   = openjp3d.encode(vol, EncodeParams(), opts);
    test_assert_true(numel(cs) >= 2, 'codestream too short');
    test_assert_equal(uint8(cs(1)), uint8(hex2dec('FF')));
    test_assert_equal(uint8(cs(2)), uint8(hex2dec('4F')));
end

function do_lossy_test()
    vol  = rng_volume([8 16 16], 80, 8, false);
    opts.Prec = 8; opts.Sgnd = false;
    p    = EncodeParams('Filter', 1, 'TargetRate', 2.0);
    cs   = openjp3d.encode(vol, p, opts);
    [out, ~] = openjp3d.decode(cs);
    test_assert_equal(size(out), [8 16 16]);
    test_assert_true( ...
        max(abs(double(out(:)) - double(int32(vol(:))))) < 50, ...
        'lossy output too far from original');
end

function do_transcode_no_params()
    vol  = rng_volume([4 8 8], 90, 8, false);
    opts.Prec = 8; opts.Sgnd = false;
    cs   = openjp3d.encode(vol, EncodeParams(), opts);
    ht   = openjp3d.transcode_to_ht(cs);
    test_assert_true(isa(ht, 'uint8'), 'transcode did not return uint8');
    [out, ~] = openjp3d.decode(ht);
    test_assert_equal(out, int32(vol));
end

function do_transcode_with_params()
    vol  = rng_volume([4 8 8], 91, 8, false);
    opts.Prec = 8; opts.Sgnd = false;
    cs   = openjp3d.encode(vol, EncodeParams(), opts);
    ht   = openjp3d.transcode_to_ht(cs, EncodeParams());
    [out, ~] = openjp3d.decode(ht);
    test_assert_equal(out, int32(vol));
end

function do_corner_voxel_test()
    vol = rng_volume([4 8 8], 100, 8, false);
    vol(1,1,1) = int32(0);
    vol(4,8,8) = int32(255);
    vol(1,1,8) = int32(100);
    vol(4,1,1) = int32(50);
    opts.Prec = 8; opts.Sgnd = false;
    cs  = openjp3d.encode(vol, EncodeParams(), opts);
    [out, ~] = openjp3d.decode(cs);
    test_assert_equal(out(1,1,1), int32(0));
    test_assert_equal(out(4,8,8), int32(255));
    test_assert_equal(out(1,1,8), int32(100));
    test_assert_equal(out(4,1,1), int32(50));
end

function do_gradient_layout_test()
    D = 4; H = 8; W = 8;
    vol = int32(zeros(D, H, W));
    for zi = 1:D
        for yi = 1:H
            for xi = 1:W
                vol(zi,yi,xi) = int32((zi-1)*32 + (yi-1)*4 + (xi-1));
            end
        end
    end
    opts.Prec = 8; opts.Sgnd = false;
    cs  = openjp3d.encode(vol, EncodeParams(), opts);
    [out, ~] = openjp3d.decode(cs);
    test_assert_equal(out, vol);
end

function do_callback_test()
    vol = rng_volume([4 8 8], 110, 8, false);
    opts.Prec = 8; opts.Sgnd = false;
    opts.OnMessage = @(level, msg) fprintf('[%d] %s\n', level, msg);
    p   = EncodeParams('Verbose', true);
    cs  = openjp3d.encode(vol, p, opts);
    test_assert_true(isa(cs, 'uint8'), 'encode with callback did not return uint8');
end

function do_dtype_test(prec, sgnd, expected_dtype)
    vol = rng_volume([4 8 8], 120+prec, prec, sgnd);
    opts.Prec = prec; opts.Sgnd = sgnd;
    cs  = openjp3d.encode(vol, EncodeParams(), opts);
    [~, dtype] = openjp3d.decode(cs);
    test_assert_equal(dtype, expected_dtype);
end

function do_infer_class_test(vol, expected_dtype)
    % Encode without explicit Prec/Sgnd — rely on class inference.
    cs = openjp3d.encode(vol, EncodeParams());
    [~, dtype] = openjp3d.decode(cs);
    test_assert_equal(dtype, expected_dtype);
end

function do_verbose_decode_test()
    vol = rng_volume([4 8 8], 130, 8, false);
    opts.Prec = 8; opts.Sgnd = false;
    cs  = openjp3d.encode(vol, EncodeParams(), opts);
    [out, ~] = openjp3d.decode(cs, 'Verbose', true);
    test_assert_equal(out, int32(vol));
end

function do_decode_callback_test()
    vol = rng_volume([4 8 8], 140, 8, false);
    opts.Prec = 8; opts.Sgnd = false;
    cs  = openjp3d.encode(vol, EncodeParams(), opts);
    cb  = @(l,m) fprintf('[%d] %s\n', l, m);
    [out, ~] = openjp3d.decode(cs, 'OnMessage', cb);
    test_assert_equal(out, int32(vol));
end
