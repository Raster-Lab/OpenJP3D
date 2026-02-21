// Copyright (c) 2024-2026, OpenJP3D Contributors
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

package openjp3d

/*
// This file only has declarations (no definitions) because it contains
// //export directives, as required by CGo.
*/
import "C"

// ojp3d_go_callback_bridge is called from the C side (ojp3d_c_callback).
// It routes the message to the current Go-side callback, if any.
//
//export ojp3d_go_callback_bridge
func ojp3d_go_callback_bridge(level C.int, msg *C.char) {
	cbMu.Lock()
	cb := activeCallback
	cbMu.Unlock()
	if cb == nil {
		return
	}
	cb(int(level), C.GoString(msg))
}
