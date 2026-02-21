% Copyright (c) 2024-2026, OpenJP3D Contributors
% All rights reserved.
% SPDX-License-Identifier: BSD-2-Clause

function v = get_version()
% GET_VERSION  Return the OpenJP3D native library version string.
%
%   V = openjp3d.get_version()
%
%   Returns a character string such as '1.0.0'.  The library must have
%   been loaded via openjp3d.load_lib() (called automatically on first use).

    openjp3d.load_lib();
    v = openjp3d_mex('get_version');
end
