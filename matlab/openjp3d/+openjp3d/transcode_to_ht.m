% Copyright (c) 2024-2026, OpenJP3D Contributors
% All rights reserved.
% SPDX-License-Identifier: BSD-2-Clause

function out = transcode_to_ht(data, params, varargin)
% TRANSCODE_TO_HT  Transcode a JP3D codestream from EBCOT to HTJ2K.
%
%   OUT = openjp3d.transcode_to_ht(DATA)
%   OUT = openjp3d.transcode_to_ht(DATA, PARAMS)
%   OUT = openjp3d.transcode_to_ht(DATA, PARAMS, 'Name', Value, ...)
%
%   Arguments:
%     DATA   - uint8 row vector containing the source JP3D codestream.
%     PARAMS - EncodeParams object (optional).  The UseHTJ2K field is
%              forced to 1 regardless of its value.
%
%   Name-Value options:
%     'OnMessage' - Function handle @(level, msg) for codec messages.
%
%   Returns:
%     OUT - uint8 row vector containing the transcoded JP3D codestream.

    % ------------------------------------------------------------------
    % Parse arguments
    % ------------------------------------------------------------------
    if ~isa(data, 'uint8')
        error('openjp3d:badarg', ...
              'transcode_to_ht: DATA must be a uint8 vector');
    end

    if nargin < 2
        params = [];
    end

    on_message = [];
    k = 1;
    while k <= numel(varargin)
        opt = varargin{k};
        if ischar(opt) && k+1 <= numel(varargin)
            switch lower(opt)
                case 'onmessage'; on_message = varargin{k+1}; k = k+2;
                otherwise
                    error('openjp3d:badarg', ...
                          'transcode_to_ht: unknown option ''%s''', opt);
            end
        else
            error('openjp3d:badarg', ...
                  'transcode_to_ht: options must be name-value pairs');
        end
    end

    % ------------------------------------------------------------------
    % Pack params
    % ------------------------------------------------------------------
    if ~isempty(params)
        if ~isa(params, 'EncodeParams')
            error('openjp3d:badarg', ...
                  'transcode_to_ht: PARAMS must be an EncodeParams object');
        end
        cp = params.to_c();
        % Force use_htj2k = 1 (index 12 in 1-based params_int)
        cp.params_int(12) = int32(1);
        has_params  = true;
        params_int  = cp.params_int;
        target_rate = cp.target_rate;
    else
        has_params  = false;
        params_int  = int32(zeros(1, 13));
        target_rate = double(0);
    end

    % ------------------------------------------------------------------
    % Call MEX
    % ------------------------------------------------------------------
    openjp3d.load_lib();

    if ~isempty(on_message) && isa(on_message, 'function_handle')
        out = openjp3d_mex('transcode', data, has_params, ...
                           params_int, target_rate, on_message);
    else
        out = openjp3d_mex('transcode', data, has_params, ...
                           params_int, target_rate);
    end
end
