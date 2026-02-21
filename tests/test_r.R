# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

#' OpenJP3D R binding test suite
#'
#' Run with:
#'   OPENJP3D_LIBRARY=/path/to/libopenjp3d.so Rscript tests/test_r.R
#'
#' Or via CTest after configuring with -DBUILD_R_BINDINGS=ON.

library(openjp3d)

# ---------------------------------------------------------------------------
# Skip all tests when the shared library is unavailable.
# ---------------------------------------------------------------------------

lib_ok <- tryCatch({
    v <- openjp3d::get_version()
    nzchar(v)
}, error = function(e) FALSE)

if (!lib_ok) {
    message("openjp3d shared library not found - skipping R binding tests.\n",
            "Set OPENJP3D_LIBRARY or rebuild with -DBUILD_SHARED_LIBS=ON.")
    quit(status = 0L)
}

# ---------------------------------------------------------------------------
# Minimal test harness (no external dependencies)
# ---------------------------------------------------------------------------

.n_pass <- 0L
.n_fail <- 0L

.test <- function(desc, expr) {
    result <- tryCatch({
        force(expr)
        TRUE
    }, error = function(e) {
        message(sprintf("FAIL: %s\n  Error: %s", desc, conditionMessage(e)))
        FALSE
    })
    if (isTRUE(result)) {
        .n_pass <<- .n_pass + 1L
        message(sprintf("pass: %s", desc))
    } else {
        .n_fail <<- .n_fail + 1L
    }
}

.expect_true <- function(x, msg = "") {
    if (!isTRUE(x))
        stop(if (nzchar(msg)) msg else paste("Expected TRUE, got:", deparse(x)))
}

.expect_equal <- function(a, b) {
    if (!identical(a, b))
        stop(paste("Values differ.\n  got:", deparse(a[1:min(3,length(a))]),
                   "...\n  expected:", deparse(b[1:min(3,length(b))]), "..."))
}

.expect_error <- function(expr, pattern = NULL) {
    caught <- tryCatch({ expr; FALSE }, error = function(e) {
        if (!is.null(pattern))
            grepl(pattern, conditionMessage(e), ignore.case = TRUE)
        else
            TRUE
    })
    if (!isTRUE(caught))
        stop(paste("Expected error matching", deparse(pattern),
                   "but none was thrown"))
}

# Strip the dtype attribute from a decoded array before comparison.
.strip_dtype <- function(x) { attr(x, "dtype") <- NULL; x }

# ---------------------------------------------------------------------------
# Helper: create a reproducible pseudo-random integer array
# ---------------------------------------------------------------------------

.rng_volume <- function(shape, seed = 0L, prec = 32L, sgnd = TRUE) {
    n   <- prod(shape)
    set.seed(seed)
    lo  <- if (prec == 8L)  { if (sgnd) -128L else 0L } else
           if (prec == 16L) { if (sgnd) -32768L else 0L } else -2147483647L
    hi  <- if (prec == 8L)  { if (sgnd) 127L else 255L } else
           if (prec == 16L) { if (sgnd) 32767L else 65535L } else 2147483647L
    v   <- sample(lo:hi, n, replace = TRUE)
    array(as.integer(v), dim = shape)
}

# ===========================================================================
# 1. Library loading & version
# ===========================================================================

.test("get_version returns a non-empty string", {
    v <- openjp3d::get_version()
    .expect_true(is.character(v))
    .expect_true(nzchar(v))
})

.test("version has MAJOR.MINOR.PATCH format", {
    v     <- openjp3d::get_version()
    parts <- strsplit(v, "\\.")[[1L]]
    .expect_true(length(parts) == 3L)
    for (p in parts)
        .expect_true(grepl("^[0-9]+$", p))
})

# ===========================================================================
# 2. Constants
# ===========================================================================

.test("FILTER_53 == 0L", .expect_equal(openjp3d::FILTER_53, 0L))
.test("FILTER_97 == 1L", .expect_equal(openjp3d::FILTER_97, 1L))
.test("USE_HTJ2K == 1L", .expect_equal(openjp3d::USE_HTJ2K, 1L))
.test("CS_UNKNOWN == 0L", .expect_equal(openjp3d::CS_UNKNOWN, 0L))
.test("CS_SRGB == 1L",    .expect_equal(openjp3d::CS_SRGB,    1L))
.test("CS_GRAY == 2L",    .expect_equal(openjp3d::CS_GRAY,    2L))
.test("CS_YUV == 3L",     .expect_equal(openjp3d::CS_YUV,     3L))
.test("MSG_INFO == 0L",    .expect_equal(openjp3d::MSG_INFO,    0L))
.test("MSG_WARNING == 1L", .expect_equal(openjp3d::MSG_WARNING, 1L))
.test("MSG_ERROR == 2L",   .expect_equal(openjp3d::MSG_ERROR,   2L))

# ===========================================================================
# 3. EncodeParams construction
# ===========================================================================

.test("EncodeParams default construction", {
    p <- openjp3d::EncodeParams()
    .expect_true(inherits(p, "EncodeParams"))
    .expect_equal(p$tile_width, 0L)
    .expect_equal(p$num_resolutions_x, 3L)
    .expect_equal(p$filter, openjp3d::FILTER_53)
    .expect_equal(p$use_htj2k, 0L)
    .expect_equal(p$verbose, 0L)
})

.test("EncodeParams custom construction", {
    p <- openjp3d::EncodeParams(
        tile_width = 32L, tile_height = 32L, tile_depth = 16L,
        filter = openjp3d::FILTER_97,
        target_rate = 1.5,
        use_htj2k = openjp3d::USE_HTJ2K,
        verbose = TRUE
    )
    .expect_equal(p$tile_width,  32L)
    .expect_equal(p$tile_depth,  16L)
    .expect_equal(p$filter,      openjp3d::FILTER_97)
    .expect_true(abs(p$target_rate - 1.5) < 1e-9)
    .expect_equal(p$use_htj2k,   openjp3d::USE_HTJ2K)
    .expect_equal(p$verbose,     1L)
})

.test("EncodeParams _to_c produces correct integer vector", {
    p  <- openjp3d::EncodeParams()
    cp <- openjp3d:::.params_to_c(p)
    .expect_true(is.integer(cp$params_int))
    .expect_equal(length(cp$params_int), 13L)
    .expect_true(is.double(cp$target_rate))
    .expect_true(abs(cp$target_rate - 0.0) < 1e-9)
})

# ===========================================================================
# 4. Lossless round-trips -- all supported precisions
# ===========================================================================

.roundtrip <- function(vol, prec, sgnd) {
    cs      <- openjp3d::encode(vol,
                                params = openjp3d::EncodeParams(),
                                prec   = prec,
                                sgnd   = sgnd)
    .expect_true(is.raw(cs))
    decoded <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(decoded), vol)
}

.test("lossless round-trip: 8-bit unsigned (uint8)", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 1L, prec = 8L, sgnd = FALSE)
    .roundtrip(vol, 8L, FALSE)
})

.test("lossless round-trip: 8-bit signed (int8)", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 2L, prec = 8L, sgnd = TRUE)
    .roundtrip(vol, 8L, TRUE)
})

.test("lossless round-trip: 16-bit unsigned (uint16)", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 3L, prec = 16L, sgnd = FALSE)
    .roundtrip(vol, 16L, FALSE)
})

.test("lossless round-trip: 16-bit signed (int16)", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 4L, prec = 16L, sgnd = TRUE)
    .roundtrip(vol, 16L, TRUE)
})

.test("lossless round-trip: 32-bit signed (int32)", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 5L, prec = 32L, sgnd = TRUE)
    .roundtrip(vol, 32L, TRUE)
})

# ===========================================================================
# 5. Multi-component volumes
# ===========================================================================

.test("lossless round-trip: 3-component volume", {
    vol <- .rng_volume(c(4L, 8L, 8L, 3L), seed = 10L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(out), vol)
})

.test("lossless round-trip: 4-component volume", {
    vol <- .rng_volume(c(4L, 8L, 8L, 4L), seed = 11L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# 6. Single-slice edge case (D = 1)
# ===========================================================================

.test("single-slice volume (D=1) round-trips losslessly", {
    vol <- .rng_volume(c(1L, 8L, 8L), seed = 20L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# 7. Non-square dimensions
# ===========================================================================

.test("non-square volume round-trips losslessly", {
    vol <- .rng_volume(c(3L, 5L, 7L), seed = 30L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# 8. Larger volume (16 x 16 x 16)
# ===========================================================================

.test("16x16x16 uint8 volume round-trips losslessly", {
    vol <- .rng_volume(c(16L, 16L, 16L), seed = 40L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# 9. Tiled encoding
# ===========================================================================

.test("tiled encoding round-trips losslessly", {
    vol <- .rng_volume(c(8L, 16L, 16L), seed = 50L, prec = 8L, sgnd = FALSE)
    p   <- openjp3d::EncodeParams(tile_width = 8L, tile_height = 8L,
                                   tile_depth = 4L)
    cs  <- openjp3d::encode(vol, params = p, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# 10. SOC marker check
# ===========================================================================

.test("codestream starts with JP3D SOC marker 0xFF4F", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 60L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    .expect_true(length(cs) >= 2L)
    .expect_true(as.integer(cs[1L]) == 0xFFL)
    .expect_true(as.integer(cs[2L]) == 0x4FL)
})

# ===========================================================================
# 11. HTJ2K round-trip
# ===========================================================================

.test("HTJ2K round-trip is lossless (5/3 filter + USE_HTJ2K)", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 70L, prec = 8L, sgnd = FALSE)
    p   <- openjp3d::EncodeParams(use_htj2k = openjp3d::USE_HTJ2K)
    cs  <- openjp3d::encode(vol, params = p, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# 12. Lossy encoding (9/7 filter)
# ===========================================================================

.test("lossy 9/7 encoding produces a valid decodable codestream", {
    vol <- .rng_volume(c(8L, 16L, 16L), seed = 80L, prec = 8L, sgnd = FALSE)
    p   <- openjp3d::EncodeParams(filter = openjp3d::FILTER_97,
                                   target_rate = 2.0)
    cs  <- openjp3d::encode(vol, params = p, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(dim(out), c(8L, 16L, 16L))
    .expect_true(max(abs(as.integer(out) - as.integer(vol))) < 50L)
})

# ===========================================================================
# 13. transcode_to_ht
# ===========================================================================

.test("transcode_to_ht (no params) produces a decodable codestream", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 90L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    ht  <- openjp3d::transcode_to_ht(cs)
    .expect_true(is.raw(ht))
    out <- openjp3d::decode(ht)
    .expect_equal(.strip_dtype(out), vol)
})

.test("transcode_to_ht (with params) produces a decodable codestream", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 91L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    p   <- openjp3d::EncodeParams()
    ht  <- openjp3d::transcode_to_ht(cs, params = p)
    out <- openjp3d::decode(ht)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# 14. Data layout correctness
# ===========================================================================

.test("corner voxels are preserved after encode->decode", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 100L, prec = 8L, sgnd = FALSE)
    vol[1L, 1L, 1L] <- 0L
    vol[4L, 8L, 8L] <- 255L
    vol[1L, 1L, 8L] <- 100L
    vol[4L, 1L, 1L] <- 50L
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(out[1L, 1L, 1L], 0L)
    .expect_equal(out[4L, 8L, 8L], 255L)
    .expect_equal(out[1L, 1L, 8L], 100L)
    .expect_equal(out[4L, 1L, 1L], 50L)
})

.test("gradient pattern is preserved (data layout correctness)", {
    d <- 4L; h <- 8L; w <- 8L
    vol <- array(0L, dim = c(d, h, w))
    for (zi in seq_len(d))
        for (yi in seq_len(h))
            for (xi in seq_len(w))
                vol[zi, yi, xi] <- as.integer(
                    (zi - 1L) * 32L + (yi - 1L) * 4L + (xi - 1L))
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# 15. Message callback
# ===========================================================================

.test("message callback can be registered without error", {
    vol      <- .rng_volume(c(4L, 8L, 8L), seed = 110L, prec = 8L,
                             sgnd = FALSE)
    messages <- character(0L)
    p        <- openjp3d::EncodeParams(verbose = TRUE)
    cs       <- openjp3d::encode(
        vol, params = p, prec = 8L, sgnd = FALSE,
        on_message = function(level, msg) {
            messages <<- c(messages, msg)
        }
    )
    .expect_true(is.raw(cs))
})

# ===========================================================================
# 16. decode() dtype attribute
# ===========================================================================

.test("decoded uint8 volume carries dtype='uint8'", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 120L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs)
    .expect_equal(attr(out, "dtype"), "uint8")
})

.test("decoded int16 volume carries dtype='int16'", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 121L, prec = 16L, sgnd = TRUE)
    cs  <- openjp3d::encode(vol, prec = 16L, sgnd = TRUE)
    out <- openjp3d::decode(cs)
    .expect_equal(attr(out, "dtype"), "int16")
})

.test("decoded int32 volume carries dtype='int32'", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 122L, prec = 32L, sgnd = TRUE)
    cs  <- openjp3d::encode(vol, prec = 32L, sgnd = TRUE)
    out <- openjp3d::decode(cs)
    .expect_equal(attr(out, "dtype"), "int32")
})

# ===========================================================================
# 17. Error handling
# ===========================================================================

.test("encode() rejects non-array input", {
    .expect_error(openjp3d::encode(1:10), "must be an R array")
})

.test("encode() rejects 2-D input", {
    v <- array(0L, dim = c(2L, 3L))
    .expect_error(openjp3d::encode(v), "3-D")
})

.test("decode() rejects non-raw input", {
    .expect_error(openjp3d::decode(as.integer(c(0xFF, 0x4F))),
                  "raw vector")
})

.test("decode() fails on invalid codestream", {
    bad <- as.raw(c(0x00, 0x01, 0x02, 0x03))
    .expect_error(openjp3d::decode(bad))
})

.test("transcode_to_ht() rejects non-raw input", {
    .expect_error(openjp3d::transcode_to_ht(as.integer(1:10)),
                  "raw vector")
})

# ===========================================================================
# 18. verbose decode
# ===========================================================================

.test("decode with verbose=TRUE does not crash", {
    vol <- .rng_volume(c(4L, 8L, 8L), seed = 130L, prec = 8L, sgnd = FALSE)
    cs  <- openjp3d::encode(vol, prec = 8L, sgnd = FALSE)
    out <- openjp3d::decode(cs, verbose = TRUE)
    .expect_equal(.strip_dtype(out), vol)
})

# ===========================================================================
# Summary
# ===========================================================================

message(sprintf("\nResult: %d passed, %d failed", .n_pass, .n_fail))
if (.n_fail > 0L)
    quit(status = 1L)
