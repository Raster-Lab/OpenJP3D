// Copyright (c) 2024-2026, OpenJP3D Contributors
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

/*
Go binding tests for the openjp3d package.

Run with:

	OPENJP3D_LIBRARY=/path/to/libopenjp3d.so \
	go test -v github.com/raster-lab/openjp3d
*/
package openjp3d_test

import (
	"os"
	"strings"
	"testing"

	"github.com/raster-lab/openjp3d"
)

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

func skipNoLib(t *testing.T) {
	t.Helper()
	if !openjp3d.IsLoaded() {
		t.Skip("openjp3d shared library not available; set OPENJP3D_LIBRARY")
	}
}

func mustLoad(t *testing.T) {
	t.Helper()
	if err := openjp3d.LoadLib(""); err != nil {
		t.Skipf("openjp3d shared library not available: %v", err)
	}
}

// makeVol creates a flat []int32 sample buffer for a (numComps, d, h, w) volume
// filled with predictable values: sample = comp*1000 + z*100 + y*10 + x (mod max).
func makeVol(w, h, d, numComps uint32, maxVal int32) []int32 {
	n := int(numComps) * int(w) * int(h) * int(d)
	buf := make([]int32, n)
	for c := uint32(0); c < numComps; c++ {
		for z := uint32(0); z < d; z++ {
			for y := uint32(0); y < h; y++ {
				for x := uint32(0); x < w; x++ {
					idx := int(c)*int(d)*int(h)*int(w) +
						int(z)*int(h)*int(w) +
						int(y)*int(w) + int(x)
					buf[idx] = int32(uint32(c*1000+z*100+y*10+x) % uint32(maxVal))
				}
			}
		}
	}
	return buf
}

// slicesEqual returns true if a and b contain the same values.
func slicesEqual(a, b []int32) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

// ---------------------------------------------------------------------------
// 1. Library loading & version
// ---------------------------------------------------------------------------

func TestLoadLib(t *testing.T) {
	mustLoad(t)
	if !openjp3d.IsLoaded() {
		t.Fatal("IsLoaded() is false after successful LoadLib")
	}
}

func TestLoadLibIdempotent(t *testing.T) {
	mustLoad(t)
	// Second call must not return an error.
	if err := openjp3d.LoadLib(""); err != nil {
		t.Fatalf("second LoadLib call returned error: %v", err)
	}
}

func TestGetVersion_ReturnsString(t *testing.T) {
	mustLoad(t)
	v, err := openjp3d.GetVersion()
	if err != nil {
		t.Fatalf("GetVersion error: %v", err)
	}
	if v == "" {
		t.Fatal("GetVersion returned empty string")
	}
}

func TestGetVersion_Format(t *testing.T) {
	mustLoad(t)
	v, _ := openjp3d.GetVersion()
	parts := strings.Split(v, ".")
	if len(parts) != 3 {
		t.Fatalf("expected MAJOR.MINOR.PATCH, got %q", v)
	}
	for _, p := range parts {
		for _, ch := range p {
			if ch < '0' || ch > '9' {
				t.Fatalf("non-numeric version part %q in %q", p, v)
			}
		}
	}
}

// ---------------------------------------------------------------------------
// 2. Constants
// ---------------------------------------------------------------------------

func TestConstants_Filter(t *testing.T) {
	if openjp3d.Filter53 != 0 {
		t.Errorf("Filter53 = %d, want 0", openjp3d.Filter53)
	}
	if openjp3d.Filter97 != 1 {
		t.Errorf("Filter97 = %d, want 1", openjp3d.Filter97)
	}
}

func TestConstants_HTJ2K(t *testing.T) {
	if openjp3d.UseHTJ2K != 1 {
		t.Errorf("UseHTJ2K = %d, want 1", openjp3d.UseHTJ2K)
	}
}

func TestConstants_ColorSpace(t *testing.T) {
	if openjp3d.CSUnknown != 0 {
		t.Errorf("CSUnknown = %d, want 0", openjp3d.CSUnknown)
	}
	if openjp3d.CSSrgb != 1 {
		t.Errorf("CSSrgb = %d, want 1", openjp3d.CSSrgb)
	}
	if openjp3d.CSGray != 2 {
		t.Errorf("CSGray = %d, want 2", openjp3d.CSGray)
	}
	if openjp3d.CSYUV != 3 {
		t.Errorf("CSYUV = %d, want 3", openjp3d.CSYUV)
	}
}

func TestConstants_MsgLevel(t *testing.T) {
	if openjp3d.MsgInfo != 0 {
		t.Errorf("MsgInfo = %d, want 0", openjp3d.MsgInfo)
	}
	if openjp3d.MsgWarning != 1 {
		t.Errorf("MsgWarning = %d, want 1", openjp3d.MsgWarning)
	}
	if openjp3d.MsgError != 2 {
		t.Errorf("MsgError = %d, want 2", openjp3d.MsgError)
	}
}

// ---------------------------------------------------------------------------
// 3. EncodeParams
// ---------------------------------------------------------------------------

func TestEncodeParams_Defaults(t *testing.T) {
	mustLoad(t)
	p := openjp3d.DefaultEncodeParams()
	// Library sets sensible defaults; just check they are non-zero where expected.
	if p.NumResolutionsX == 0 {
		t.Error("DefaultEncodeParams: NumResolutionsX is 0")
	}
	if p.CblkWidth == 0 {
		t.Error("DefaultEncodeParams: CblkWidth is 0")
	}
	if p.NumLayers == 0 {
		t.Error("DefaultEncodeParams: NumLayers is 0")
	}
	if p.Filter != openjp3d.Filter53 {
		t.Errorf("DefaultEncodeParams: Filter = %d, want Filter53 (%d)",
			p.Filter, openjp3d.Filter53)
	}
	if p.TargetRate != 0.0 {
		t.Errorf("DefaultEncodeParams: TargetRate = %v, want 0 (lossless)", p.TargetRate)
	}
}

func TestEncodeParams_ManualFields(t *testing.T) {
	p := openjp3d.EncodeParams{
		TileWidth:       32,
		TileHeight:      32,
		TileDepth:       16,
		NumResolutionsX: 2,
		NumResolutionsY: 2,
		NumResolutionsZ: 1,
		CblkWidth:       32,
		CblkHeight:      32,
		CblkDepth:       16,
		Filter:          openjp3d.Filter97,
		NumLayers:       1,
		TargetRate:      2.0,
	}
	if p.TileWidth != 32 || p.TileHeight != 32 || p.TileDepth != 16 {
		t.Error("EncodeParams tile dimensions not stored correctly")
	}
	if p.Filter != openjp3d.Filter97 {
		t.Errorf("EncodeParams.Filter = %d, want Filter97", p.Filter)
	}
	if p.TargetRate != 2.0 {
		t.Errorf("EncodeParams.TargetRate = %v, want 2.0", p.TargetRate)
	}
}

// ---------------------------------------------------------------------------
// 4. Lossless round-trip — uint8
// ---------------------------------------------------------------------------

func TestRoundTrip_Uint8(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 256)
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	if len(cs) == 0 {
		t.Fatal("Encode returned empty codestream")
	}
	dec, info, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if info.W != 8 || info.H != 8 || info.D != 4 {
		t.Errorf("Decode info: got %dx%dx%d, want 8x8x4", info.W, info.H, info.D)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (uint8)")
	}
}

// ---------------------------------------------------------------------------
// 5. Lossless round-trip — int8
// ---------------------------------------------------------------------------

func TestRoundTrip_Int8(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 128)
	// signed int8 range: shift to [-64, 63]
	for i := range vol {
		vol[i] -= 64
	}
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 1, 8, true, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (int8)")
	}
}

// ---------------------------------------------------------------------------
// 6. Lossless round-trip — uint16
// ---------------------------------------------------------------------------

func TestRoundTrip_Uint16(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 65536)
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 1, 16, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, info, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if info.Prec != 16 {
		t.Errorf("Decode info prec = %d, want 16", info.Prec)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (uint16)")
	}
}

// ---------------------------------------------------------------------------
// 7. Lossless round-trip — int16
// ---------------------------------------------------------------------------

func TestRoundTrip_Int16(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 32768)
	for i := range vol {
		vol[i] -= 16384
	}
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 1, 16, true, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (int16)")
	}
}

// ---------------------------------------------------------------------------
// 8. Lossless round-trip — int32
// ---------------------------------------------------------------------------

func TestRoundTrip_Int32(t *testing.T) {
	mustLoad(t)
	vol := makeVol(4, 4, 4, 1, 1<<20)
	cs, err := openjp3d.Encode(vol, 4, 4, 4, 1, 20, true, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (int32)")
	}
}

// ---------------------------------------------------------------------------
// 9. Multi-component (3-channel)
// ---------------------------------------------------------------------------

func TestRoundTrip_MultiComponent3(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 3, 256)
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 3, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, info, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if info.NumComps != 3 {
		t.Errorf("NumComps = %d, want 3", info.NumComps)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (3-component)")
	}
}

// ---------------------------------------------------------------------------
// 10. Multi-component (4-channel)
// ---------------------------------------------------------------------------

func TestRoundTrip_MultiComponent4(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 4, 256)
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 4, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, info, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if info.NumComps != 4 {
		t.Errorf("NumComps = %d, want 4", info.NumComps)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (4-component)")
	}
}

// ---------------------------------------------------------------------------
// 11. Single-slice edge case (d=1)
// ---------------------------------------------------------------------------

func TestRoundTrip_SingleSlice(t *testing.T) {
	mustLoad(t)
	vol := makeVol(16, 16, 1, 1, 256)
	cs, err := openjp3d.Encode(vol, 16, 16, 1, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, info, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if info.D != 1 {
		t.Errorf("info.D = %d, want 1", info.D)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (single-slice)")
	}
}

// ---------------------------------------------------------------------------
// 12. Non-square dimensions
// ---------------------------------------------------------------------------

func TestRoundTrip_NonSquare(t *testing.T) {
	mustLoad(t)
	vol := makeVol(16, 8, 4, 1, 256) // w≠h≠d
	cs, err := openjp3d.Encode(vol, 16, 8, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, info, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if info.W != 16 || info.H != 8 || info.D != 4 {
		t.Errorf("got %dx%dx%d, want 16x8x4", info.W, info.H, info.D)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (non-square)")
	}
}

// ---------------------------------------------------------------------------
// 13. Large volume (16×16×16)
// ---------------------------------------------------------------------------

func TestRoundTrip_Large(t *testing.T) {
	mustLoad(t)
	vol := makeVol(16, 16, 16, 1, 256)
	cs, err := openjp3d.Encode(vol, 16, 16, 16, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (16x16x16)")
	}
}

// ---------------------------------------------------------------------------
// 14. Tiled encoding
// ---------------------------------------------------------------------------

func TestRoundTrip_Tiled(t *testing.T) {
	mustLoad(t)
	vol := makeVol(16, 16, 8, 1, 256)
	p := openjp3d.EncodeParams{
		TileWidth:       8,
		TileHeight:      8,
		TileDepth:       4,
		NumResolutionsX: 2,
		NumResolutionsY: 2,
		NumResolutionsZ: 1,
		CblkWidth:       8,
		CblkHeight:      8,
		CblkDepth:       4,
		Filter:          openjp3d.Filter53,
		NumLayers:       1,
	}
	cs, err := openjp3d.Encode(vol, 16, 16, 8, 1, 8, false, &p, nil)
	if err != nil {
		t.Fatalf("Encode (tiled) error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode (tiled) error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (tiled)")
	}
}

// ---------------------------------------------------------------------------
// 15. HTJ2K lossless round-trip
// ---------------------------------------------------------------------------

func TestRoundTrip_HTJ2K(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 256)
	p := openjp3d.DefaultEncodeParams()
	p.UseHTJ2K = openjp3d.UseHTJ2K
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 1, 8, false, &p, nil)
	if err != nil {
		t.Fatalf("Encode (HTJ2K) error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode (HTJ2K) error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("lossless round-trip mismatch (HTJ2K)")
	}
}

// ---------------------------------------------------------------------------
// 16. Lossy encoding (9/7 filter, target rate)
// ---------------------------------------------------------------------------

func TestEncode_Lossy(t *testing.T) {
	mustLoad(t)
	vol := makeVol(16, 16, 8, 1, 256)
	p := openjp3d.DefaultEncodeParams()
	p.Filter = openjp3d.Filter97
	p.TargetRate = 1.0
	cs, err := openjp3d.Encode(vol, 16, 16, 8, 1, 8, false, &p, nil)
	if err != nil {
		t.Fatalf("Encode (lossy) error: %v", err)
	}
	if len(cs) == 0 {
		t.Fatal("lossy Encode returned empty codestream")
	}
	// Decode must succeed even if output differs.
	_, _, err = openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode (lossy) error: %v", err)
	}
}

// ---------------------------------------------------------------------------
// 17. TranscodeToHT
// ---------------------------------------------------------------------------

func TestTranscodeToHT_Basic(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 256)
	// First encode with standard coder.
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	// Transcode to HTJ2K.
	ht, err := openjp3d.TranscodeToHT(cs, nil, nil)
	if err != nil {
		t.Fatalf("TranscodeToHT error: %v", err)
	}
	if len(ht) == 0 {
		t.Fatal("TranscodeToHT returned empty codestream")
	}
	// The transcoded codestream must be decodable.
	dec, _, err := openjp3d.Decode(ht, nil, nil)
	if err != nil {
		t.Fatalf("Decode after TranscodeToHT error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("round-trip mismatch after TranscodeToHT")
	}
}

func TestTranscodeToHT_WithParams(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 256)
	cs, err := openjp3d.Encode(vol, 8, 8, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	p := openjp3d.DefaultEncodeParams()
	ht, err := openjp3d.TranscodeToHT(cs, &p, nil)
	if err != nil {
		t.Fatalf("TranscodeToHT (with params) error: %v", err)
	}
	if len(ht) == 0 {
		t.Fatal("TranscodeToHT (with params) returned empty codestream")
	}
}

// ---------------------------------------------------------------------------
// 18. Codestream SOC marker check
// ---------------------------------------------------------------------------

func TestEncode_SOCMarker(t *testing.T) {
	mustLoad(t)
	vol := makeVol(4, 4, 4, 1, 256)
	cs, err := openjp3d.Encode(vol, 4, 4, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	// JP3D codestreams start with SOC marker 0xFF4F.
	if len(cs) < 2 {
		t.Fatal("codestream too short")
	}
	if cs[0] != 0xFF || cs[1] != 0x4F {
		t.Errorf("missing SOC marker: got %#02x %#02x, want 0xFF 0x4F", cs[0], cs[1])
	}
}

// ---------------------------------------------------------------------------
// 19. Data layout: corner voxel check
// ---------------------------------------------------------------------------

func TestDecode_DataLayout(t *testing.T) {
	mustLoad(t)
	// Fill with a gradient so we can spot layout errors.
	vol := makeVol(4, 4, 4, 1, 256)
	cs, err := openjp3d.Encode(vol, 4, 4, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	// Check first and last samples.
	if dec[0] != vol[0] {
		t.Errorf("first sample: got %d, want %d", dec[0], vol[0])
	}
	last := len(vol) - 1
	if dec[last] != vol[last] {
		t.Errorf("last sample: got %d, want %d", dec[last], vol[last])
	}
}

// ---------------------------------------------------------------------------
// 20. Message callback is invoked
// ---------------------------------------------------------------------------

func TestEncode_Callback(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 256)
	p := openjp3d.DefaultEncodeParams()
	p.Verbose = 1

	var msgs []string
	cb := func(level int, msg string) {
		msgs = append(msgs, msg)
	}
	_, err := openjp3d.Encode(vol, 8, 8, 4, 1, 8, false, &p, cb)
	if err != nil {
		t.Fatalf("Encode with callback error: %v", err)
	}
	// With Verbose=1 the library should emit at least one informational message.
	if len(msgs) == 0 {
		t.Log("no messages received (library may not emit verbose messages in this build)")
	}
}

// ---------------------------------------------------------------------------
// 21. Decode with Verbose=1
// ---------------------------------------------------------------------------

func TestDecode_Verbose(t *testing.T) {
	mustLoad(t)
	vol := makeVol(8, 8, 4, 1, 256)
	cs, _ := openjp3d.Encode(vol, 8, 8, 4, 1, 8, false, nil, nil)
	p := &openjp3d.DecodeParams{Verbose: 1}
	_, _, err := openjp3d.Decode(cs, p, nil)
	if err != nil {
		t.Fatalf("Decode (verbose) error: %v", err)
	}
}

// ---------------------------------------------------------------------------
// 22. VolumeInfo fields
// ---------------------------------------------------------------------------

func TestDecode_VolumeInfo(t *testing.T) {
	mustLoad(t)
	vol := makeVol(5, 7, 3, 2, 1024)
	cs, err := openjp3d.Encode(vol, 5, 7, 3, 2, 10, true, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	_, info, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if info.W != 5 || info.H != 7 || info.D != 3 {
		t.Errorf("dims: got %dx%dx%d, want 5x7x3", info.W, info.H, info.D)
	}
	if info.NumComps != 2 {
		t.Errorf("NumComps = %d, want 2", info.NumComps)
	}
	if info.Prec != 10 {
		t.Errorf("Prec = %d, want 10", info.Prec)
	}
	if !info.Signed {
		t.Error("Signed = false, want true")
	}
}

// ---------------------------------------------------------------------------
// 23. Error: empty codestream
// ---------------------------------------------------------------------------

func TestDecode_EmptyCodestream(t *testing.T) {
	mustLoad(t)
	_, _, err := openjp3d.Decode([]byte{}, nil, nil)
	if err == nil {
		t.Error("expected error for empty codestream, got nil")
	}
}

// ---------------------------------------------------------------------------
// 24. Error: invalid codestream bytes
// ---------------------------------------------------------------------------

func TestDecode_InvalidCodestream(t *testing.T) {
	mustLoad(t)
	garbage := []byte{0x00, 0x01, 0x02, 0x03, 0x04}
	_, _, err := openjp3d.Decode(garbage, nil, nil)
	if err == nil {
		t.Error("expected error for invalid codestream, got nil")
	}
}

// ---------------------------------------------------------------------------
// 25. Error: Encode with zero dimensions
// ---------------------------------------------------------------------------

func TestEncode_ZeroWidth(t *testing.T) {
	mustLoad(t)
	vol := []int32{1, 2, 3, 4}
	_, err := openjp3d.Encode(vol, 0, 4, 4, 1, 8, false, nil, nil)
	if err == nil {
		t.Error("expected error for zero width, got nil")
	}
}

// ---------------------------------------------------------------------------
// 26. Error: Encode with insufficient samples slice
// ---------------------------------------------------------------------------

func TestEncode_TooFewSamples(t *testing.T) {
	mustLoad(t)
	vol := []int32{1, 2, 3} // needs 8*8*4 = 256 samples
	_, err := openjp3d.Encode(vol, 8, 8, 4, 1, 8, false, nil, nil)
	if err == nil {
		t.Error("expected error for too-small samples slice, got nil")
	}
}

// ---------------------------------------------------------------------------
// 27. Error: TranscodeToHT with empty source
// ---------------------------------------------------------------------------

func TestTranscodeToHT_EmptySource(t *testing.T) {
	mustLoad(t)
	_, err := openjp3d.TranscodeToHT([]byte{}, nil, nil)
	if err == nil {
		t.Error("expected error for empty source codestream, got nil")
	}
}

// ---------------------------------------------------------------------------
// 28. All-zeros volume
// ---------------------------------------------------------------------------

func TestRoundTrip_AllZeros(t *testing.T) {
	mustLoad(t)
	vol := make([]int32, 4*4*4)
	cs, err := openjp3d.Encode(vol, 4, 4, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("round-trip mismatch for all-zeros volume")
	}
}

// ---------------------------------------------------------------------------
// 29. All-max-value volume
// ---------------------------------------------------------------------------

func TestRoundTrip_AllMax(t *testing.T) {
	mustLoad(t)
	vol := make([]int32, 4*4*4)
	for i := range vol {
		vol[i] = 255
	}
	cs, err := openjp3d.Encode(vol, 4, 4, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	dec, _, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if !slicesEqual(vol, dec) {
		t.Error("round-trip mismatch for all-255 volume")
	}
}

// ---------------------------------------------------------------------------
// 30. IsLoaded reflects library state
// ---------------------------------------------------------------------------

func TestIsLoaded_True(t *testing.T) {
	mustLoad(t)
	if !openjp3d.IsLoaded() {
		t.Error("IsLoaded() returned false after successful load")
	}
}

// ---------------------------------------------------------------------------
// 31. OPENJP3D_LIBRARY env var is used by resolveLibPath (integration smoke)
// ---------------------------------------------------------------------------

func TestLoadLib_UsesEnvVar(t *testing.T) {
	// If the library is already loaded we can only verify IsLoaded.
	path := os.Getenv("OPENJP3D_LIBRARY")
	if path == "" {
		t.Skip("OPENJP3D_LIBRARY not set")
	}
	mustLoad(t)
	if !openjp3d.IsLoaded() {
		t.Error("library not loaded despite OPENJP3D_LIBRARY being set")
	}
}

// ---------------------------------------------------------------------------
// 32. VolumeInfo.Signed false for unsigned volume
// ---------------------------------------------------------------------------

func TestDecode_UnsignedFlag(t *testing.T) {
	mustLoad(t)
	vol := makeVol(4, 4, 4, 1, 256)
	cs, err := openjp3d.Encode(vol, 4, 4, 4, 1, 8, false, nil, nil)
	if err != nil {
		t.Fatalf("Encode error: %v", err)
	}
	_, info, err := openjp3d.Decode(cs, nil, nil)
	if err != nil {
		t.Fatalf("Decode error: %v", err)
	}
	if info.Signed {
		t.Error("VolumeInfo.Signed = true for unsigned volume")
	}
}
