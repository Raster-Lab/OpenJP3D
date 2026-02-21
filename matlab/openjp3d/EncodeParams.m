% Copyright (c) 2024-2026, OpenJP3D Contributors
% All rights reserved.
% SPDX-License-Identifier: BSD-2-Clause

classdef EncodeParams
% ENCODEPARAMS  Encoder parameters for openjp3d.encode / transcode_to_ht.
%
%   P = EncodeParams()            — default lossless parameters
%   P = EncodeParams('Name', Val, ...)
%
%   Properties (all writeable):
%     TileWidth        - Tile width  in voxels (0 = whole volume, default).
%     TileHeight       - Tile height in voxels (0 = whole volume, default).
%     TileDepth        - Tile depth  in voxels (0 = whole volume, default).
%     NumResolutionsX  - DWT decomposition levels, x axis (default 3).
%     NumResolutionsY  - DWT decomposition levels, y axis (default 3).
%     NumResolutionsZ  - DWT decomposition levels, z axis (default 3).
%     CblkWidth        - Code-block width  (default 4).
%     CblkHeight       - Code-block height (default 4).
%     CblkDepth        - Code-block depth  (default 4).
%     Filter           - 0 = FILTER_53 (lossless), 1 = FILTER_97 (lossy).
%     NumLayers        - Number of quality layers (default 1).
%     TargetRate       - Target bits/sample; 0 = lossless (default).
%     UseHTJ2K         - 1 = enable HTJ2K block coder (default 0).
%     Verbose          - 1 = enable codec messages (default 0).
%
%   Methods:
%     v = to_c(P)  — Returns a struct with fields params_int (int32(13))
%                    and target_rate (double), matching the C struct layout.

    properties
        TileWidth        int32 = int32(0)
        TileHeight       int32 = int32(0)
        TileDepth        int32 = int32(0)
        NumResolutionsX  int32 = int32(3)
        NumResolutionsY  int32 = int32(3)
        NumResolutionsZ  int32 = int32(3)
        CblkWidth        int32 = int32(4)
        CblkHeight       int32 = int32(4)
        CblkDepth        int32 = int32(4)
        Filter           int32 = int32(0)   % FILTER_53
        NumLayers        int32 = int32(1)
        TargetRate       double = double(0) % 0 = lossless
        UseHTJ2K         int32 = int32(0)
        Verbose          int32 = int32(0)
    end

    methods
        function obj = EncodeParams(varargin)
        % EncodeParams  Construct with optional name-value pairs.
        %
        %   Recognised names (case-insensitive):
        %     TileWidth, TileHeight, TileDepth,
        %     NumResolutionsX, NumResolutionsY, NumResolutionsZ,
        %     CblkWidth, CblkHeight, CblkDepth,
        %     Filter, NumLayers, TargetRate, UseHTJ2K, Verbose

            if mod(numel(varargin), 2) ~= 0
                error('openjp3d:badarg', ...
                      'EncodeParams: arguments must be name-value pairs');
            end
            for k = 1 : 2 : numel(varargin)
                name = varargin{k};
                val  = varargin{k+1};
                switch lower(name)
                    case 'tilewidth';        obj.TileWidth        = int32(val);
                    case 'tileheight';       obj.TileHeight       = int32(val);
                    case 'tiledepth';        obj.TileDepth        = int32(val);
                    case 'numresolutionsx';  obj.NumResolutionsX  = int32(val);
                    case 'numresolutionsy';  obj.NumResolutionsY  = int32(val);
                    case 'numresolutionsz';  obj.NumResolutionsZ  = int32(val);
                    case 'cblkwidth';        obj.CblkWidth        = int32(val);
                    case 'cblkheight';       obj.CblkHeight       = int32(val);
                    case 'cblkdepth';        obj.CblkDepth        = int32(val);
                    case 'filter';           obj.Filter           = int32(val);
                    case 'numlayers';        obj.NumLayers        = int32(val);
                    case 'targetrate';       obj.TargetRate       = double(val);
                    case 'usehtj2k';         obj.UseHTJ2K         = int32(val);
                    case 'verbose';          obj.Verbose          = int32(logical(val));
                    otherwise
                        error('openjp3d:badarg', ...
                              'EncodeParams: unknown property ''%s''', name);
                end
            end
        end

        function s = to_c(obj)
        % TO_C  Pack parameters into the layout expected by openjp3d_mex.
        %
        %   S = P.to_c()
        %
        %   Returns a struct with:
        %     S.params_int   — int32(13) vector:
        %                        [tw, th, td, rx, ry, rz,
        %                         bw, bh, bd, filter, num_layers,
        %                         use_htj2k, verbose]
        %     S.target_rate  — double scalar

            s.params_int = int32([ ...
                obj.TileWidth,       obj.TileHeight,      obj.TileDepth, ...
                obj.NumResolutionsX, obj.NumResolutionsY, obj.NumResolutionsZ, ...
                obj.CblkWidth,       obj.CblkHeight,      obj.CblkDepth, ...
                obj.Filter,          obj.NumLayers, ...
                obj.UseHTJ2K,        obj.Verbose]);
            s.target_rate = double(obj.TargetRate);
        end
    end
end
