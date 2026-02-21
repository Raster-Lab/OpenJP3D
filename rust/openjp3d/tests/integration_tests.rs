// Copyright (c) 2024-2026, OpenJP3D Contributors
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

//! Integration tests for the openjp3d Rust bindings.
//!
//! Run with:
//!
//! ```bash
//! OPENJP3D_LIBRARY=/path/to/libopenjp3d.so cargo test
//! ```

use openjp3d::{
    decode, default_encode_params, encode, get_version, is_loaded, load_lib, transcode_to_ht,
    DecodeParams, EncodeParams, FILTER_53, FILTER_97, MSG_ERROR, MSG_INFO, MSG_WARNING,
    CS_GRAY, CS_SRGB, CS_UNKNOWN, CS_YUV, USE_HTJ2K,
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Skip the test if the shared library is not available.
fn require_lib() -> bool {
    if !is_loaded() {
        let _ = load_lib(None);
    }
    if !is_loaded() {
        eprintln!("openjp3d shared library not available; set OPENJP3D_LIBRARY to run tests");
        return false;
    }
    true
}

/// Create a flat `Vec<i32>` sample buffer for a `(num_comps, d, h, w)` volume
/// filled with predictable values: `sample = (comp*1000 + z*100 + y*10 + x) % max_val`.
fn make_vol(w: u32, h: u32, d: u32, num_comps: u32, max_val: i32) -> Vec<i32> {
    let n = (num_comps * w * h * d) as usize;
    let mut buf = vec![0i32; n];
    for c in 0..num_comps {
        for z in 0..d {
            for y in 0..h {
                for x in 0..w {
                    let idx = (c * d * h * w + z * h * w + y * w + x) as usize;
                    buf[idx] = ((c * 1000 + z * 100 + y * 10 + x) as i32) % max_val;
                }
            }
        }
    }
    buf
}

// ---------------------------------------------------------------------------
// 1. Library loading
// ---------------------------------------------------------------------------

#[test]
fn test_load_lib() {
    if !require_lib() {
        return;
    }
    assert!(is_loaded(), "IsLoaded should be true after load_lib");
}

#[test]
fn test_load_lib_idempotent() {
    if !require_lib() {
        return;
    }
    // Second call must not error.
    let r = load_lib(None);
    assert!(r.is_ok(), "second load_lib call returned error: {:?}", r);
}

// ---------------------------------------------------------------------------
// 2. Version
// ---------------------------------------------------------------------------

#[test]
fn test_get_version_nonempty() {
    if !require_lib() {
        return;
    }
    let v = get_version().expect("get_version failed");
    assert!(!v.is_empty(), "get_version returned empty string");
}

#[test]
fn test_get_version_format() {
    if !require_lib() {
        return;
    }
    let v = get_version().unwrap();
    let parts: Vec<&str> = v.split('.').collect();
    assert_eq!(parts.len(), 3, "expected MAJOR.MINOR.PATCH, got {:?}", v);
    for p in &parts {
        assert!(
            p.chars().all(|c| c.is_ascii_digit()),
            "non-numeric version part {:?} in {:?}",
            p,
            v
        );
    }
}

// ---------------------------------------------------------------------------
// 3. Constants
// ---------------------------------------------------------------------------

#[test]
fn test_constants_filter() {
    assert_eq!(FILTER_53, 0);
    assert_eq!(FILTER_97, 1);
}

#[test]
fn test_constants_htj2k() {
    assert_eq!(USE_HTJ2K, 1);
}

#[test]
fn test_constants_color_space() {
    assert_eq!(CS_UNKNOWN, 0u32);
    assert_eq!(CS_SRGB, 1u32);
    assert_eq!(CS_GRAY, 2u32);
    assert_eq!(CS_YUV, 3u32);
}

#[test]
fn test_constants_msg_level() {
    assert_eq!(MSG_INFO, 0);
    assert_eq!(MSG_WARNING, 1);
    assert_eq!(MSG_ERROR, 2);
}

// ---------------------------------------------------------------------------
// 4. EncodeParams defaults
// ---------------------------------------------------------------------------

#[test]
fn test_encode_params_defaults() {
    if !require_lib() {
        return;
    }
    let p = default_encode_params().expect("default_encode_params failed");
    assert!(p.num_resolutions_x > 0, "NumResolutionsX is 0");
    assert!(p.cblk_width > 0, "CblkWidth is 0");
    assert!(p.num_layers > 0, "NumLayers is 0");
    assert_eq!(p.filter, FILTER_53, "default filter should be FILTER_53");
    assert_eq!(p.target_rate, 0.0f32, "default target_rate should be 0 (lossless)");
    assert_eq!(p.use_htj2k, 0, "default use_htj2k should be 0");
}

#[test]
fn test_encode_params_manual_fields() {
    let p = EncodeParams {
        tile_width: 32,
        tile_height: 32,
        tile_depth: 16,
        num_resolutions_x: 2,
        num_resolutions_y: 2,
        num_resolutions_z: 1,
        cblk_width: 32,
        cblk_height: 32,
        cblk_depth: 16,
        filter: FILTER_97,
        num_layers: 1,
        target_rate: 2.0,
        use_htj2k: 0,
        verbose: 0,
    };
    assert_eq!(p.tile_width, 32);
    assert_eq!(p.tile_depth, 16);
    assert_eq!(p.filter, FILTER_97);
    assert!((p.target_rate - 2.0f32).abs() < f32::EPSILON);
}

// ---------------------------------------------------------------------------
// 5. Lossless round-trip — uint8
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_uint8() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 1, 256);
    let cs = encode(&vol, 8, 8, 4, 1, 8, false, None, None).expect("encode failed");
    assert!(!cs.is_empty(), "encode returned empty codestream");
    let (dec, info) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(info.w, 8, "wrong width");
    assert_eq!(info.h, 8, "wrong height");
    assert_eq!(info.d, 4, "wrong depth");
    assert_eq!(vol, dec, "lossless round-trip mismatch (uint8)");
}

// ---------------------------------------------------------------------------
// 6. Lossless round-trip — int8
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_int8() {
    if !require_lib() {
        return;
    }
    let mut vol = make_vol(8, 8, 4, 1, 128);
    for v in &mut vol {
        *v -= 64;
    }
    let cs = encode(&vol, 8, 8, 4, 1, 8, true, None, None).expect("encode failed");
    let (dec, _) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(vol, dec, "lossless round-trip mismatch (int8)");
}

// ---------------------------------------------------------------------------
// 7. Lossless round-trip — uint16
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_uint16() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 1, 65536);
    let cs = encode(&vol, 8, 8, 4, 1, 16, false, None, None).expect("encode failed");
    let (dec, info) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(info.prec, 16, "prec mismatch");
    assert_eq!(vol, dec, "lossless round-trip mismatch (uint16)");
}

// ---------------------------------------------------------------------------
// 8. Lossless round-trip — int16
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_int16() {
    if !require_lib() {
        return;
    }
    let mut vol = make_vol(8, 8, 4, 1, 32768);
    for v in &mut vol {
        *v -= 16384;
    }
    let cs = encode(&vol, 8, 8, 4, 1, 16, true, None, None).expect("encode failed");
    let (dec, _) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(vol, dec, "lossless round-trip mismatch (int16)");
}

// ---------------------------------------------------------------------------
// 9. Lossless round-trip — int32
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_int32() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(4, 4, 4, 1, 1 << 20);
    let cs = encode(&vol, 4, 4, 4, 1, 20, true, None, None).expect("encode failed");
    let (dec, _) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(vol, dec, "lossless round-trip mismatch (int32)");
}

// ---------------------------------------------------------------------------
// 10. Multi-component (3-channel)
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_multicomponent3() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 3, 256);
    let cs = encode(&vol, 8, 8, 4, 3, 8, false, None, None).expect("encode failed");
    let (dec, info) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(info.num_comps, 3, "num_comps mismatch");
    assert_eq!(vol, dec, "lossless round-trip mismatch (3-component)");
}

// ---------------------------------------------------------------------------
// 11. Multi-component (4-channel)
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_multicomponent4() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 4, 256);
    let cs = encode(&vol, 8, 8, 4, 4, 8, false, None, None).expect("encode failed");
    let (dec, info) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(info.num_comps, 4, "num_comps mismatch");
    assert_eq!(vol, dec, "lossless round-trip mismatch (4-component)");
}

// ---------------------------------------------------------------------------
// 12. Single-slice edge case (d=1)
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_single_slice() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(16, 16, 1, 1, 256);
    let cs = encode(&vol, 16, 16, 1, 1, 8, false, None, None).expect("encode failed");
    let (dec, info) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(info.d, 1, "info.d should be 1");
    assert_eq!(vol, dec, "lossless round-trip mismatch (single-slice)");
}

// ---------------------------------------------------------------------------
// 13. Non-square dimensions
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_nonsquare() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(16, 8, 4, 1, 256);
    let cs = encode(&vol, 16, 8, 4, 1, 8, false, None, None).expect("encode failed");
    let (dec, info) = decode(&cs, None, None).expect("decode failed");
    assert_eq!((info.w, info.h, info.d), (16, 8, 4), "dimension mismatch");
    assert_eq!(vol, dec, "lossless round-trip mismatch (non-square)");
}

// ---------------------------------------------------------------------------
// 14. Large volume (16×16×16)
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_large() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(16, 16, 16, 1, 256);
    let cs = encode(&vol, 16, 16, 16, 1, 8, false, None, None).expect("encode failed");
    let (dec, _) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(vol, dec, "lossless round-trip mismatch (16x16x16)");
}

// ---------------------------------------------------------------------------
// 15. Tiled encoding
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_tiled() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(16, 16, 8, 1, 256);
    let p = EncodeParams {
        tile_width: 8,
        tile_height: 8,
        tile_depth: 4,
        num_resolutions_x: 2,
        num_resolutions_y: 2,
        num_resolutions_z: 1,
        cblk_width: 8,
        cblk_height: 8,
        cblk_depth: 4,
        filter: FILTER_53,
        num_layers: 1,
        target_rate: 0.0,
        use_htj2k: 0,
        verbose: 0,
    };
    let cs = encode(&vol, 16, 16, 8, 1, 8, false, Some(&p), None).expect("encode (tiled) failed");
    let (dec, _) = decode(&cs, None, None).expect("decode (tiled) failed");
    assert_eq!(vol, dec, "lossless round-trip mismatch (tiled)");
}

// ---------------------------------------------------------------------------
// 16. HTJ2K lossless round-trip
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_htj2k() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 1, 256);
    let mut p = default_encode_params().unwrap();
    p.use_htj2k = USE_HTJ2K;
    let cs = encode(&vol, 8, 8, 4, 1, 8, false, Some(&p), None).expect("encode (HTJ2K) failed");
    let (dec, _) = decode(&cs, None, None).expect("decode (HTJ2K) failed");
    assert_eq!(vol, dec, "lossless round-trip mismatch (HTJ2K)");
}

// ---------------------------------------------------------------------------
// 17. Lossy encoding (9/7 filter, target rate)
// ---------------------------------------------------------------------------

#[test]
fn test_encode_lossy() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(16, 16, 8, 1, 256);
    let mut p = default_encode_params().unwrap();
    p.filter = FILTER_97;
    p.target_rate = 1.0;
    let cs = encode(&vol, 16, 16, 8, 1, 8, false, Some(&p), None)
        .expect("encode (lossy) failed");
    assert!(!cs.is_empty(), "lossy encode returned empty codestream");
    let (_, _) = decode(&cs, None, None).expect("decode (lossy) failed");
}

// ---------------------------------------------------------------------------
// 18. TranscodeToHT — basic
// ---------------------------------------------------------------------------

#[test]
fn test_transcode_to_ht_basic() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 1, 256);
    let cs = encode(&vol, 8, 8, 4, 1, 8, false, None, None).expect("encode failed");
    let ht = transcode_to_ht(&cs, None, None).expect("transcode_to_ht failed");
    assert!(!ht.is_empty(), "transcode_to_ht returned empty codestream");
    let (dec, _) = decode(&ht, None, None).expect("decode after transcode failed");
    assert_eq!(vol, dec, "round-trip mismatch after TranscodeToHT");
}

// ---------------------------------------------------------------------------
// 19. TranscodeToHT — with params
// ---------------------------------------------------------------------------

#[test]
fn test_transcode_to_ht_with_params() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 1, 256);
    let cs = encode(&vol, 8, 8, 4, 1, 8, false, None, None).expect("encode failed");
    let p = default_encode_params().unwrap();
    let ht = transcode_to_ht(&cs, Some(&p), None).expect("transcode_to_ht (with params) failed");
    assert!(!ht.is_empty(), "transcode_to_ht (with params) returned empty codestream");
}

// ---------------------------------------------------------------------------
// 20. SOC marker check
// ---------------------------------------------------------------------------

#[test]
fn test_encode_soc_marker() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(4, 4, 4, 1, 256);
    let cs = encode(&vol, 4, 4, 4, 1, 8, false, None, None).expect("encode failed");
    assert!(cs.len() >= 2, "codestream too short");
    assert_eq!(
        (cs[0], cs[1]),
        (0xFF, 0x4F),
        "missing SOC marker 0xFF4F"
    );
}

// ---------------------------------------------------------------------------
// 21. Data layout: corner voxel check
// ---------------------------------------------------------------------------

#[test]
fn test_decode_data_layout() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(4, 4, 4, 1, 256);
    let cs = encode(&vol, 4, 4, 4, 1, 8, false, None, None).expect("encode failed");
    let (dec, _) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(dec[0], vol[0], "first sample mismatch");
    let last = vol.len() - 1;
    assert_eq!(dec[last], vol[last], "last sample mismatch");
}

// ---------------------------------------------------------------------------
// 22. Message callback is invoked
// ---------------------------------------------------------------------------

#[test]
fn test_encode_callback() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 1, 256);
    let mut p = default_encode_params().unwrap();
    p.verbose = 1;

    let msgs: std::sync::Arc<std::sync::Mutex<Vec<String>>> =
        std::sync::Arc::new(std::sync::Mutex::new(Vec::new()));
    let msgs_clone = msgs.clone();
    let cb = move |_level: i32, msg: &str| {
        msgs_clone.lock().unwrap().push(msg.to_owned());
    };

    let _ = encode(&vol, 8, 8, 4, 1, 8, false, Some(&p), Some(&cb));
    // With Verbose=1 the library may emit messages; just verify no panic.
}

// ---------------------------------------------------------------------------
// 23. Decode with verbose=1
// ---------------------------------------------------------------------------

#[test]
fn test_decode_verbose() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(8, 8, 4, 1, 256);
    let cs = encode(&vol, 8, 8, 4, 1, 8, false, None, None).unwrap();
    let p = DecodeParams { verbose: 1 };
    let (_, _) = decode(&cs, Some(&p), None).expect("decode (verbose) failed");
}

// ---------------------------------------------------------------------------
// 24. VolumeInfo fields
// ---------------------------------------------------------------------------

#[test]
fn test_decode_volume_info() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(5, 7, 3, 2, 1024);
    let cs = encode(&vol, 5, 7, 3, 2, 10, true, None, None).expect("encode failed");
    let (_, info) = decode(&cs, None, None).expect("decode failed");
    assert_eq!((info.w, info.h, info.d), (5, 7, 3), "dimension mismatch");
    assert_eq!(info.num_comps, 2, "num_comps mismatch");
    assert_eq!(info.prec, 10, "prec mismatch");
    assert!(info.signed, "signed flag mismatch");
}

// ---------------------------------------------------------------------------
// 25. VolumeInfo.signed is false for unsigned volume
// ---------------------------------------------------------------------------

#[test]
fn test_decode_unsigned_flag() {
    if !require_lib() {
        return;
    }
    let vol = make_vol(4, 4, 4, 1, 256);
    let cs = encode(&vol, 4, 4, 4, 1, 8, false, None, None).expect("encode failed");
    let (_, info) = decode(&cs, None, None).expect("decode failed");
    assert!(!info.signed, "VolumeInfo.signed should be false for unsigned volume");
}

// ---------------------------------------------------------------------------
// 26. Error: empty codestream
// ---------------------------------------------------------------------------

#[test]
fn test_decode_empty_codestream() {
    if !require_lib() {
        return;
    }
    let r = decode(&[], None, None);
    assert!(r.is_err(), "expected error for empty codestream, got Ok");
}

// ---------------------------------------------------------------------------
// 27. Error: invalid codestream bytes
// ---------------------------------------------------------------------------

#[test]
fn test_decode_invalid_codestream() {
    if !require_lib() {
        return;
    }
    let garbage = vec![0x00u8, 0x01, 0x02, 0x03, 0x04];
    let r = decode(&garbage, None, None);
    assert!(r.is_err(), "expected error for invalid codestream, got Ok");
}

// ---------------------------------------------------------------------------
// 28. Error: encode with zero dimensions
// ---------------------------------------------------------------------------

#[test]
fn test_encode_zero_width() {
    if !require_lib() {
        return;
    }
    let vol = vec![1i32, 2, 3, 4];
    let r = encode(&vol, 0, 4, 4, 1, 8, false, None, None);
    assert!(r.is_err(), "expected error for zero width, got Ok");
}

// ---------------------------------------------------------------------------
// 29. Error: encode with too-few samples
// ---------------------------------------------------------------------------

#[test]
fn test_encode_too_few_samples() {
    if !require_lib() {
        return;
    }
    let vol = vec![1i32, 2, 3]; // needs 8*8*4 = 256 samples
    let r = encode(&vol, 8, 8, 4, 1, 8, false, None, None);
    assert!(r.is_err(), "expected error for too-few samples, got Ok");
}

// ---------------------------------------------------------------------------
// 30. Error: transcode_to_ht with empty source
// ---------------------------------------------------------------------------

#[test]
fn test_transcode_to_ht_empty_source() {
    if !require_lib() {
        return;
    }
    let r = transcode_to_ht(&[], None, None);
    assert!(r.is_err(), "expected error for empty source codestream, got Ok");
}

// ---------------------------------------------------------------------------
// 31. All-zeros volume
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_all_zeros() {
    if !require_lib() {
        return;
    }
    let vol = vec![0i32; 4 * 4 * 4];
    let cs = encode(&vol, 4, 4, 4, 1, 8, false, None, None).expect("encode failed");
    let (dec, _) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(vol, dec, "round-trip mismatch for all-zeros volume");
}

// ---------------------------------------------------------------------------
// 32. All-max-value volume
// ---------------------------------------------------------------------------

#[test]
fn test_roundtrip_all_max() {
    if !require_lib() {
        return;
    }
    let vol = vec![255i32; 4 * 4 * 4];
    let cs = encode(&vol, 4, 4, 4, 1, 8, false, None, None).expect("encode failed");
    let (dec, _) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(vol, dec, "round-trip mismatch for all-255 volume");
}

// ---------------------------------------------------------------------------
// 33. is_loaded reflects library state
// ---------------------------------------------------------------------------

#[test]
fn test_is_loaded() {
    if !require_lib() {
        return;
    }
    assert!(is_loaded(), "is_loaded() should be true after successful load");
}

// ---------------------------------------------------------------------------
// 34. OPENJP3D_LIBRARY env var is used
// ---------------------------------------------------------------------------

#[test]
fn test_load_lib_uses_env_var() {
    let path = std::env::var("OPENJP3D_LIBRARY").unwrap_or_default();
    if path.is_empty() {
        eprintln!("OPENJP3D_LIBRARY not set — skipping env-var test");
        return;
    }
    let r = load_lib(None);
    assert!(r.is_ok(), "load_lib failed despite OPENJP3D_LIBRARY being set: {:?}", r);
    assert!(is_loaded());
}
