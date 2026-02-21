# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

#' @title OpenJP3D — R Bindings for the JP3D Volumetric Codec
#'
#' @description
#' Pure R bindings for the OpenJP3D shared library.  Encode and decode 3-D
#' image volumes using JPEG 2000 Part 10 (JP3D) directly from R arrays
#' without requiring a Python or Julia interpreter.
#'
#' @section Constants:
#' \describe{
#'   \item{\code{FILTER_53}}{Lossless 5/3 integer lifting filter (default).}
#'   \item{\code{FILTER_97}}{Lossy 9/7 floating-point lifting filter.}
#'   \item{\code{USE_HTJ2K}}{Enable the HTJ2K high-throughput block coder.}
#'   \item{\code{CS_UNKNOWN}, \code{CS_SRGB}, \code{CS_GRAY}, \code{CS_YUV}}{Colour spaces.}
#'   \item{\code{MSG_INFO}, \code{MSG_WARNING}, \code{MSG_ERROR}}{Callback severity levels.}
#' }
#'
#' @name openjp3d-package
NULL

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

#' Lossless 5/3 integer lifting filter (default).
#' @export
FILTER_53 <- 0L

#' Lossy 9/7 floating-point lifting filter.
#' @export
FILTER_97 <- 1L

#' Flag to enable the HTJ2K high-throughput block coder.
#' @export
USE_HTJ2K <- 1L

#' Unknown colour space.
#' @export
CS_UNKNOWN <- 0L

#' sRGB colour space.
#' @export
CS_SRGB <- 1L

#' Greyscale colour space.
#' @export
CS_GRAY <- 2L

#' YCbCr (YUV) colour space.
#' @export
CS_YUV <- 3L

#' Informational message severity.
#' @export
MSG_INFO <- 0L

#' Warning message severity.
#' @export
MSG_WARNING <- 1L

#' Error message severity.
#' @export
MSG_ERROR <- 2L

# ---------------------------------------------------------------------------
# EncodeParams — encoder parameter list constructor
# ---------------------------------------------------------------------------

#' Create encoder parameters for \code{\link{encode}}.
#'
#' All fields mirror \code{opj_jp3d_enc_params_t} with R-friendly defaults.
#'
#' @param tile_width,tile_height,tile_depth Tile dimensions (0 = whole volume).
#' @param num_resolutions_x,num_resolutions_y,num_resolutions_z
#'   DWT decomposition levels per axis (default 3).
#' @param cblk_width,cblk_height,cblk_depth Code-block dimensions (default 4).
#' @param filter \code{FILTER_53} (lossless, default) or \code{FILTER_97} (lossy).
#' @param num_layers Number of quality layers (default 1).
#' @param target_rate Target bits/sample; 0 means lossless (default).
#' @param use_htj2k Set to \code{USE_HTJ2K} to enable the HT block coder.
#' @param verbose Enable codec info messages (\code{FALSE} by default).
#' @return A list of class \code{"EncodeParams"}.
#' @export
EncodeParams <- function(tile_width        = 0L,
                         tile_height       = 0L,
                         tile_depth        = 0L,
                         num_resolutions_x = 3L,
                         num_resolutions_y = 3L,
                         num_resolutions_z = 3L,
                         cblk_width        = 4L,
                         cblk_height       = 4L,
                         cblk_depth        = 4L,
                         filter            = FILTER_53,
                         num_layers        = 1L,
                         target_rate       = 0.0,
                         use_htj2k         = 0L,
                         verbose           = FALSE) {
    p <- list(
        tile_width        = as.integer(tile_width),
        tile_height       = as.integer(tile_height),
        tile_depth        = as.integer(tile_depth),
        num_resolutions_x = as.integer(num_resolutions_x),
        num_resolutions_y = as.integer(num_resolutions_y),
        num_resolutions_z = as.integer(num_resolutions_z),
        cblk_width        = as.integer(cblk_width),
        cblk_height       = as.integer(cblk_height),
        cblk_depth        = as.integer(cblk_depth),
        filter            = as.integer(filter),
        num_layers        = as.integer(num_layers),
        target_rate       = as.double(target_rate),
        use_htj2k         = as.integer(use_htj2k),
        verbose           = as.integer(isTRUE(verbose))
    )
    class(p) <- "EncodeParams"
    p
}

# Internal helper: pack EncodeParams into the integer(13) + double(1) pair
# expected by the C wrapper.
.params_to_c <- function(p) {
    if (!inherits(p, "EncodeParams"))
        stop("params must be an EncodeParams object")
    pi <- c(p$tile_width, p$tile_height, p$tile_depth,
            p$num_resolutions_x, p$num_resolutions_y, p$num_resolutions_z,
            p$cblk_width, p$cblk_height, p$cblk_depth,
            p$filter, p$num_layers, p$use_htj2k, p$verbose)
    list(params_int = as.integer(pi),
         target_rate = as.double(p$target_rate))
}

# ---------------------------------------------------------------------------
# get_version()
# ---------------------------------------------------------------------------

#' Return the native OpenJP3D library version string.
#'
#' @return A character string such as \code{"1.0.0"}.
#' @export
get_version <- function() {
    .Call("ojp3d_get_version", PACKAGE = "openjp3d")
}

# ---------------------------------------------------------------------------
# encode()
# ---------------------------------------------------------------------------

#' Encode a 3-D volume to a JP3D codestream.
#'
#' @param volume An integer array of shape \code{(D, H, W)} or
#'   \code{(D, H, W, C)}.  Values must fit in the range implied by
#'   \code{params$prec} and \code{params$sgnd}.  Use
#'   \code{storage.mode(volume) <- "integer"} if needed.
#' @param params An \code{\link{EncodeParams}} object.  Created with default
#'   lossless settings when \code{NULL}.
#' @param prec Bit depth per sample (8, 16, or 32; default 32).  Only needs
#'   to be set when the natural precision of the data is less than 32 bits
#'   (e.g. \code{prec = 8} for byte-valued CT slices).
#' @param sgnd Signed flag: \code{TRUE} (default) or \code{FALSE} (unsigned).
#' @param on_message Optional callback \code{function(level, message)} for
#'   codec messages.  \code{level} is one of \code{MSG_INFO} (0),
#'   \code{MSG_WARNING} (1), or \code{MSG_ERROR} (2).
#' @return A \code{raw} vector containing the JP3D codestream.
#' @export
encode <- function(volume,
                   params     = NULL,
                   prec       = 32L,
                   sgnd       = TRUE,
                   on_message = NULL) {

    if (!is.array(volume))
        stop("volume must be an R array (use array() or dim<-)")

    dv <- dim(volume)
    if (length(dv) == 3L) {
        d <- dv[1L]; h <- dv[2L]; w <- dv[3L]; numcomps <- 1L
        dim(volume) <- c(dv, 1L)
    } else if (length(dv) == 4L) {
        d <- dv[1L]; h <- dv[2L]; w <- dv[3L]; numcomps <- dv[4L]
    } else {
        stop(sprintf(
            "Expected a 3-D (D,H,W) or 4-D (D,H,W,C) array, got %d-D", length(dv)
        ))
    }

    # Ensure integer storage
    storage.mode(volume) <- "integer"

    if (is.null(params))
        params <- EncodeParams()

    cp  <- .params_to_c(params)
    prec_i <- as.integer(prec)
    sgnd_i <- as.integer(isTRUE(sgnd))

    # Convert R column-major (d,h,w,numcomps) to C row-major (w,h,d) per
    # component.  In R: dim = (D, H, W, C); fastest index = D.
    # We want C layout: data[z*w*h + y*w + x] — x fastest.
    # For a single (D,H,W) slice: aperm(slice, c(3,2,1)) -> (W,H,D); the
    # resulting in-memory layout is W-major (x fastest). ✓
    # Assemble the flat integer vector for all components.
    n_voxels <- as.integer(w) * as.integer(h) * as.integer(d)
    flat <- integer(n_voxels * numcomps)
    for (c_idx in seq_len(numcomps)) {
        # Use array() to explicitly preserve dimensions (avoids R's drop=TRUE
        # collapsing length-1 dimensions when D=1 or H=1 or W=1).
        slice <- array(volume[, , , c_idx], dim = c(d, h, w))  # (D, H, W)
        perm  <- aperm(slice, c(3L, 2L, 1L))  # (W, H, D)
        flat[((c_idx - 1L) * n_voxels + 1L):(c_idx * n_voxels)] <- as.integer(perm)
    }

    cb_arg <- if (is.function(on_message)) on_message else NULL

    .Call("ojp3d_encode",
          flat,
          as.integer(w), as.integer(h), as.integer(d),
          as.integer(numcomps),
          prec_i, sgnd_i,
          cp$params_int, cp$target_rate,
          cb_arg,
          PACKAGE = "openjp3d")
}

# ---------------------------------------------------------------------------
# decode()
# ---------------------------------------------------------------------------

#' Decode a JP3D codestream to an R array.
#'
#' @param data A \code{raw} vector containing the JP3D codestream.
#' @param verbose Enable codec info messages (\code{FALSE} by default).
#' @param on_message Optional callback \code{function(level, message)}.
#' @return An integer array of shape \code{(D, H, W)} for single-component
#'   volumes or \code{(D, H, W, C)} for multi-component volumes.  The values
#'   are in the range implied by the encoded bit-depth and signedness.
#'   A \code{"dtype"} attribute records the original type as a string
#'   (e.g. \code{"uint8"}, \code{"int16"}, \code{"int32"}).
#' @export
decode <- function(data,
                   verbose    = FALSE,
                   on_message = NULL) {

    if (!is.raw(data))
        stop("data must be a raw vector (the JP3D codestream bytes)")

    cb_arg <- if (is.function(on_message)) on_message else NULL

    res <- .Call("ojp3d_decode",
                 data,
                 as.logical(verbose),
                 cb_arg,
                 PACKAGE = "openjp3d")

    w        <- res$w
    h        <- res$h
    d        <- res$d
    numcomps <- res$numcomps
    prec     <- res$prec
    sgnd     <- res$sgnd
    n_voxels <- w * h * d

    # Determine dtype string
    dtype <- if (prec <= 8L) {
        if (sgnd) "int8" else "uint8"
    } else if (prec <= 16L) {
        if (sgnd) "int16" else "uint16"
    } else {
        "int32"
    }

    # Reconstruct per-component arrays and convert from C row-major to R
    # column-major.  C layout: data[z*w*h + y*w + x] stored as (W,H,D) in C
    # order.  reshape to (W,H,D), then aperm to (D,H,W).
    raw_data <- res$data  # flat integer vector, all components concatenated

    if (numcomps == 1L) {
        flat   <- raw_data[1L:n_voxels]
        block  <- array(flat, dim = c(w, h, d))         # (W, H, D) row-major
        result <- aperm(block, c(3L, 2L, 1L))           # (D, H, W) column-major
    } else {
        result <- array(0L, dim = c(d, h, w, numcomps))
        for (c_idx in seq_len(numcomps)) {
            flat  <- raw_data[((c_idx - 1L) * n_voxels + 1L):(c_idx * n_voxels)]
            block <- array(flat, dim = c(w, h, d))
            # Assign explicitly with preserved shape
            result[, , , c_idx] <- array(aperm(block, c(3L, 2L, 1L)),
                                         dim = c(d, h, w))
        }
    }

    attr(result, "dtype") <- dtype
    result
}

# ---------------------------------------------------------------------------
# transcode_to_ht()
# ---------------------------------------------------------------------------

#' Losslessly transcode a JP3D codestream from EBCOT to HTJ2K.
#'
#' @param data A \code{raw} vector containing the source JP3D codestream.
#' @param params An optional \code{\link{EncodeParams}} object.  The
#'   \code{use_htj2k} field is forced to \code{USE_HTJ2K} regardless of its
#'   value.  Pass \code{NULL} (default) to derive parameters from the source
#'   codestream header.
#' @param on_message Optional callback \code{function(level, message)}.
#' @return A \code{raw} vector containing the transcoded JP3D codestream.
#' @export
transcode_to_ht <- function(data,
                             params     = NULL,
                             on_message = NULL) {

    if (!is.raw(data))
        stop("data must be a raw vector (the JP3D codestream bytes)")

    if (!is.null(params) && !inherits(params, "EncodeParams"))
        stop("params must be an EncodeParams object or NULL")

    has_params <- !is.null(params)
    if (has_params) {
        cp <- .params_to_c(params)
        pi <- cp$params_int
        # Force use_htj2k to 1 (R 1-based index 12 = use_htj2k field).
        # params_int layout: [tw, th, td, rx, ry, rz, bw, bh, bd,
        #                     filter(10), num_layers(11), use_htj2k(12), verbose(13)]
        pi[12L] <- 1L
        rate <- cp$target_rate
    } else {
        pi   <- integer(13L)
        rate <- 0.0
    }

    cb_arg <- if (is.function(on_message)) on_message else NULL

    .Call("ojp3d_transcode",
          data,
          as.logical(has_params),
          pi, rate,
          cb_arg,
          PACKAGE = "openjp3d")
}
