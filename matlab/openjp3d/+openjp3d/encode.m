% Copyright (c) 2024-2026, OpenJP3D Contributors
% All rights reserved.
% SPDX-License-Identifier: BSD-2-Clause

function data = encode(volume, params, options)
% ENCODE  Encode a 3-D volume to a JP3D codestream.
%
%   DATA = openjp3d.encode(VOLUME)
%   DATA = openjp3d.encode(VOLUME, PARAMS)
%   DATA = openjp3d.encode(VOLUME, PARAMS, 'Name', Value, ...)
%
%   Arguments:
%     VOLUME    - Numeric array of shape [D H W] or [D H W C].
%                 Supported MATLAB classes: int8, uint8, int16, uint16,
%                 int32.  Precision and signedness are inferred from the
%                 class unless overridden by Prec / Sgnd.
%     PARAMS    - EncodeParams object (optional; defaults to lossless).
%
%   Name-Value options:
%     'Prec'      - Bit depth (8, 16, or 32).  Default: inferred from class.
%     'Sgnd'      - Signed flag (true/false).   Default: inferred from class.
%     'OnMessage' - Function handle @(level, msg) for codec messages.
%
%   Returns:
%     DATA - uint8 row vector containing the JP3D codestream bytes.

    % ------------------------------------------------------------------
    % Parse arguments
    % ------------------------------------------------------------------
    if nargin < 2 || isempty(params)
        params = EncodeParams();
    end
    if ~isa(params, 'EncodeParams')
        error('openjp3d:badarg', ...
              'encode: PARAMS must be an EncodeParams object');
    end

    % Defaults for name-value options
    prec_override = [];
    sgnd_override = [];
    on_message    = [];

    if nargin >= 3
        if isstruct(options)
            % called via encode(vol, params, struct(...))
            if isfield(options, 'Prec'),      prec_override = options.Prec; end
            if isfield(options, 'Sgnd'),      sgnd_override = options.Sgnd; end
            if isfield(options, 'OnMessage'), on_message    = options.OnMessage; end
        else
            % Legacy positional or error
            error('openjp3d:badarg', ...
                  'encode: third argument must be a struct of options');
        end
    end

    % ------------------------------------------------------------------
    % Validate volume
    % ------------------------------------------------------------------
    if ~isnumeric(volume)
        error('openjp3d:badarg', ...
              'encode: VOLUME must be a numeric array');
    end
    nd = ndims(volume);
    if nd == 2
        error('openjp3d:badarg', ...
              'encode: VOLUME must be a 3-D [D H W] or 4-D [D H W C] array');
    end
    if nd == 3
        sz       = size(volume);
        D        = sz(1);  H = sz(2);  W = sz(3);
        numcomps = 1;
    elseif nd == 4
        sz       = size(volume);
        D        = sz(1);  H = sz(2);  W = sz(3);  numcomps = sz(4);
    else
        error('openjp3d:badarg', ...
              'encode: VOLUME must be 3-D or 4-D, got %d-D', nd);
    end

    % ------------------------------------------------------------------
    % Infer prec / sgnd from MATLAB class
    % ------------------------------------------------------------------
    cls = class(volume);
    switch cls
        case 'int8';   default_prec =  8; default_sgnd = true;
        case 'uint8';  default_prec =  8; default_sgnd = false;
        case 'int16';  default_prec = 16; default_sgnd = true;
        case 'uint16'; default_prec = 16; default_sgnd = false;
        case 'int32';  default_prec = 32; default_sgnd = true;
        otherwise
            % Accept double / single for convenience; treat as int32
            default_prec = 32;
            default_sgnd = true;
    end

    if isempty(prec_override)
        prec = default_prec;
    else
        prec = double(prec_override);
    end
    if isempty(sgnd_override)
        sgnd = default_sgnd;
    else
        sgnd = logical(sgnd_override);
    end

    % ------------------------------------------------------------------
    % Convert to int32 for MEX
    % ------------------------------------------------------------------
    volume = int32(volume);

    % ------------------------------------------------------------------
    % Column-major (D,H,W) -> C row-major (x fastest).
    % permute([D H W] -> [W H D]) so MATLAB column-major iteration is
    % x-fastest (same as C row-major with x = fastest index).
    % ------------------------------------------------------------------
    n_voxels = double(W) * double(H) * double(D);
    flat     = int32(zeros(1, n_voxels * numcomps));

    for c = 1 : numcomps
        if numcomps == 1
            slice = reshape(volume, [D H W]);
        else
            slice = reshape(volume(:,:,:,c), [D H W]);
        end
        perm    = permute(slice, [3 2 1]);          % [W H D]
        offset  = (c-1) * n_voxels;
        flat(offset+1 : offset+n_voxels) = int32(perm(:));
    end

    % ------------------------------------------------------------------
    % Pack params
    % ------------------------------------------------------------------
    cp = params.to_c();

    % ------------------------------------------------------------------
    % Build MEX argument list
    % ------------------------------------------------------------------
    mex_args = { ...
        flat, ...
        int32(W), int32(H), int32(D), int32(numcomps), ...
        int32(prec), int32(sgnd), ...
        cp.params_int, cp.target_rate ...
    };

    if ~isempty(on_message) && isa(on_message, 'function_handle')
        mex_args{end+1} = on_message;
    end

    % ------------------------------------------------------------------
    % Call MEX
    % ------------------------------------------------------------------
    openjp3d.load_lib();
    data = openjp3d_mex('encode', mex_args{:});
end
