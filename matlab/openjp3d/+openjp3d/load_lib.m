% Copyright (c) 2024-2026, OpenJP3D Contributors
% All rights reserved.
% SPDX-License-Identifier: BSD-2-Clause

function load_lib(path)
% LOAD_LIB  Load the OpenJP3D native shared library at runtime.
%
%   openjp3d.load_lib(PATH)
%
%   PATH is the absolute path to the shared library
%   (e.g. '/usr/local/lib/libopenjp3d.so').
%
%   This function is called automatically by the other API functions when
%   the library has not yet been loaded.  The search order is:
%     1. OPENJP3D_LIBRARY environment variable.
%     2. A 'lib' subdirectory next to this .m file.
%     3. OS default loader search path (LD_LIBRARY_PATH / PATH / DYLD_...).
%
%   You only need to call load_lib() directly when none of the above
%   locations contain the library.

    persistent loaded;

    % If already loaded and called without explicit path, return early.
    if nargin < 1 || isempty(path)
        if ~isempty(loaded) && loaded
            return;
        end
        path = openjp3d_find_lib_();
    end

    openjp3d_mex('load_lib', path);
    loaded = true;
end

% -------------------------------------------------------------------------
% Internal: search for the shared library
% -------------------------------------------------------------------------

function p = openjp3d_find_lib_()
    % 1. Environment variable
    env = getenv('OPENJP3D_LIBRARY');
    if ~isempty(env) && exist(env, 'file')
        p = env;
        return;
    end

    % Determine platform-specific library name
    if ispc()
        names = {'openjp3d.dll'};
    elseif ismac()
        names = {'libopenjp3d.dylib', 'libopenjp3d.so'};
    else
        names = {'libopenjp3d.so', 'libopenjp3d.so.1'};
    end

    % 2. lib/ next to this file
    this_dir = fileparts(mfilename('fullpath'));
    lib_dir  = fullfile(fileparts(this_dir), 'lib');
    for k = 1:numel(names)
        candidate = fullfile(lib_dir, names{k});
        if exist(candidate, 'file')
            p = candidate;
            return;
        end
    end

    % 3. Let the OS loader find it (pass bare name; dlopen searches PATH)
    if ispc()
        p = 'openjp3d.dll';
    elseif ismac()
        p = 'libopenjp3d.dylib';
    else
        p = 'libopenjp3d.so';
    end
end
