% Copyright (c) 2024-2026, OpenJP3D Contributors
% All rights reserved.
% SPDX-License-Identifier: BSD-2-Clause

function [volume, dtype] = decode(data, varargin)
% DECODE  Decode a JP3D codestream to a MATLAB array.
%
%   VOLUME = openjp3d.decode(DATA)
%   VOLUME = openjp3d.decode(DATA, 'Name', Value, ...)
%   [VOLUME, DTYPE] = openjp3d.decode(...)
%
%   Arguments:
%     DATA - uint8 row vector containing the JP3D codestream bytes.
%
%   Name-Value options:
%     'Verbose'   - Enable codec messages (false by default).
%     'OnMessage' - Function handle @(level, msg).
%
%   Returns:
%     VOLUME - int32 array of shape [D H W] (single component) or
%              [D H W C] (multi-component).
%     DTYPE  - String describing the original sample type, e.g.
%              'uint8', 'int8', 'uint16', 'int16', 'int32'.

    % ------------------------------------------------------------------
    % Parse arguments
    % ------------------------------------------------------------------
    if ~isa(data, 'uint8')
        error('openjp3d:badarg', ...
              'decode: DATA must be a uint8 vector (the JP3D codestream bytes)');
    end

    verbose    = false;
    on_message = [];

    k = 1;
    while k <= numel(varargin)
        opt = varargin{k};
        if ischar(opt) && k+1 <= numel(varargin)
            switch lower(opt)
                case 'verbose';   verbose    = logical(varargin{k+1}); k = k+2;
                case 'onmessage'; on_message = varargin{k+1};          k = k+2;
                otherwise
                    error('openjp3d:badarg', ...
                          'decode: unknown option ''%s''', opt);
            end
        else
            error('openjp3d:badarg', ...
                  'decode: options must be name-value pairs');
        end
    end

    % ------------------------------------------------------------------
    % Call MEX
    % ------------------------------------------------------------------
    openjp3d.load_lib();

    if ~isempty(on_message) && isa(on_message, 'function_handle')
        res = openjp3d_mex('decode', data, verbose, on_message);
    else
        res = openjp3d_mex('decode', data, verbose);
    end

    % ------------------------------------------------------------------
    % Reconstruct volume from C row-major to MATLAB column-major
    % ------------------------------------------------------------------
    W        = res.w;
    H        = res.h;
    D        = res.d;
    numcomps = res.numcomps;
    prec     = res.prec;
    sgnd     = res.sgnd;
    n_voxels = W * H * D;

    % Determine dtype string
    if prec <= 8
        if sgnd;  dtype = 'int8';   else dtype = 'uint8';  end
    elseif prec <= 16
        if sgnd;  dtype = 'int16';  else dtype = 'uint16'; end
    else
        dtype = 'int32';
    end

    raw_data = res.data;   % int32 flat vector, all components concatenated

    if numcomps == 1
        flat   = raw_data(1 : n_voxels);
        block  = reshape(flat, [W H D]);     % (W, H, D) — C row-major
        volume = permute(block, [3 2 1]);    % (D, H, W) — MATLAB column-major
    else
        volume = int32(zeros(D, H, W, numcomps));
        for c = 1 : numcomps
            off   = (c-1) * n_voxels;
            flat  = raw_data(off+1 : off+n_voxels);
            block = reshape(flat, [W H D]);
            volume(:,:,:,c) = permute(block, [3 2 1]);
        end
    end
end
