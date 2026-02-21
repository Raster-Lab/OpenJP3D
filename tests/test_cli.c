/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file test_cli.c
 * @brief End-to-end CLI tests for Phase 5 command-line tools.
 *
 * Uses the library API to generate test data, then shells out to the
 * CLI tools and verifies round-trip correctness.
 */

#include "openjp3d.h"
#include "opj_raw_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define NULL_DEVICE "NUL"
#else
#define NULL_DEVICE "/dev/null"
#endif

static int tests_run    = 0;
static int tests_passed = 0;

#define RUN_TEST(fn)                                   \
    do {                                               \
        tests_run++;                                   \
        if (fn()) {                                    \
            tests_passed++;                            \
        } else {                                       \
            fprintf(stderr, "FAIL: %s\n", #fn);       \
        }                                              \
    } while (0)

/* ----------------------------------------------------------------------- */
/* Paths to CLI tools (set via argv)                                       */
/* ----------------------------------------------------------------------- */
static char compress_path[1024];
static char decompress_path[1024];
static char dump_path[1024];
static char transcode_path[1024];
static char temp_dir[512];

/* ----------------------------------------------------------------------- */
/* Helpers                                                                 */
/* ----------------------------------------------------------------------- */

/** Create a raw volume file with known data for testing. */
static int create_test_raw(const char *path, uint32_t w, uint32_t h,
                           uint32_t d, uint32_t prec, int32_t sgnd)
{
    opj_volume_t *vol = opj_jp3d_create_volume(1, w, h, d, prec, sgnd);
    if (!vol) return 0;

    size_t n = (size_t)w * h * d;
    for (size_t i = 0; i < n; i++) {
        vol->comps[0].data[i] = (int32_t)(i % (1u << prec));
    }

    int ok = opj_raw_io_write(vol, path);
    opj_jp3d_destroy_volume(vol);
    return ok;
}

/** Compare two binary files; returns 1 if identical, 0 if different. */
static int files_equal(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb");
    FILE *fb = fopen(b, "rb");
    if (!fa || !fb) {
        if (fa) fclose(fa);
        if (fb) fclose(fb);
        return 0;
    }

    int eq = 1;
    for (;;) {
        int ca = fgetc(fa);
        int cb = fgetc(fb);
        if (ca != cb) { eq = 0; break; }
        if (ca == EOF) break;
    }

    fclose(fa);
    fclose(fb);
    return eq;
}

/** Return 1 if file exists and is non-empty. */
static int file_nonempty(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fclose(fp);
    return len > 0;
}

/* ----------------------------------------------------------------------- */
/* Test cases                                                              */
/* ----------------------------------------------------------------------- */

/** 1. Basic compress/decompress round-trip (8-bit, 4x4x4). */
static int test_roundtrip_8bit(void)
{
    char raw_in[512], jp3d[512], raw_out[512];
    snprintf(raw_in,  sizeof(raw_in),  "%s/cli_rt8_in.raw", temp_dir);
    snprintf(jp3d,    sizeof(jp3d),    "%s/cli_rt8.jp3d",   temp_dir);
    snprintf(raw_out, sizeof(raw_out), "%s/cli_rt8_out.raw", temp_dir);

    if (!create_test_raw(raw_in, 4, 4, 4, 8, 0)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 4 -H 4 -D 4 -p 8",
             compress_path, raw_in, jp3d);
    if (system(cmd) != 0) return 0;
    if (!file_nonempty(jp3d)) return 0;

    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s",
             decompress_path, jp3d, raw_out);
    if (system(cmd) != 0) return 0;

    return files_equal(raw_in, raw_out);
}

/** 2. 16-bit compress/decompress round-trip. */
static int test_roundtrip_16bit(void)
{
    char raw_in[512], jp3d[512], raw_out[512];
    snprintf(raw_in,  sizeof(raw_in),  "%s/cli_rt16_in.raw", temp_dir);
    snprintf(jp3d,    sizeof(jp3d),    "%s/cli_rt16.jp3d",   temp_dir);
    snprintf(raw_out, sizeof(raw_out), "%s/cli_rt16_out.raw", temp_dir);

    if (!create_test_raw(raw_in, 8, 8, 4, 16, 0)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 8 -H 8 -D 4 -p 16",
             compress_path, raw_in, jp3d);
    if (system(cmd) != 0) return 0;

    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s",
             decompress_path, jp3d, raw_out);
    if (system(cmd) != 0) return 0;

    return files_equal(raw_in, raw_out);
}

/** 3. HTJ2K compress/decompress round-trip. */
static int test_roundtrip_htj2k(void)
{
    char raw_in[512], jp3d[512], raw_out[512];
    snprintf(raw_in,  sizeof(raw_in),  "%s/cli_rtht_in.raw", temp_dir);
    snprintf(jp3d,    sizeof(jp3d),    "%s/cli_rtht.jp3d",   temp_dir);
    snprintf(raw_out, sizeof(raw_out), "%s/cli_rtht_out.raw", temp_dir);

    if (!create_test_raw(raw_in, 8, 8, 4, 8, 0)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 8 -H 8 -D 4 -p 8 -H2K",
             compress_path, raw_in, jp3d);
    if (system(cmd) != 0) return 0;

    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s",
             decompress_path, jp3d, raw_out);
    if (system(cmd) != 0) return 0;

    return files_equal(raw_in, raw_out);
}

/** 4. Signed samples round-trip. */
static int test_roundtrip_signed(void)
{
    char raw_in[512], jp3d[512], raw_out[512];
    snprintf(raw_in,  sizeof(raw_in),  "%s/cli_rts_in.raw", temp_dir);
    snprintf(jp3d,    sizeof(jp3d),    "%s/cli_rts.jp3d",   temp_dir);
    snprintf(raw_out, sizeof(raw_out), "%s/cli_rts_out.raw", temp_dir);

    if (!create_test_raw(raw_in, 4, 4, 4, 8, 1)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 4 -H 4 -D 4 -p 8 -s",
             compress_path, raw_in, jp3d);
    if (system(cmd) != 0) return 0;

    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s",
             decompress_path, jp3d, raw_out);
    if (system(cmd) != 0) return 0;

    return files_equal(raw_in, raw_out);
}

/** 5. Dump tool produces output. */
static int test_dump_output(void)
{
    char raw_in[512], jp3d[512], dump_out[512];
    snprintf(raw_in,   sizeof(raw_in),   "%s/cli_dump_in.raw",  temp_dir);
    snprintf(jp3d,     sizeof(jp3d),     "%s/cli_dump.jp3d",    temp_dir);
    snprintf(dump_out, sizeof(dump_out), "%s/cli_dump_out.txt", temp_dir);

    if (!create_test_raw(raw_in, 4, 4, 4, 8, 0)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 4 -H 4 -D 4 -p 8",
             compress_path, raw_in, jp3d);
    if (system(cmd) != 0) return 0;

    snprintf(cmd, sizeof(cmd),
             "%s -i %s > %s",
             dump_path, jp3d, dump_out);
    if (system(cmd) != 0) return 0;

    if (!file_nonempty(dump_out)) return 0;

    /* Verify the dump contains expected markers */
    FILE *fp = fopen(dump_out, "r");
    if (!fp) return 0;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    if (!strstr(buf, "SOC"))   return 0;
    if (!strstr(buf, "SIZ3D")) return 0;
    if (!strstr(buf, "COD3D")) return 0;
    if (!strstr(buf, "QCD3D")) return 0;
    if (!strstr(buf, "SOT"))   return 0;
    if (!strstr(buf, "SOD"))   return 0;
    if (!strstr(buf, "EOC"))   return 0;

    return 1;
}

/** 6. Transcode EBCOT -> HTJ2K round-trip. */
static int test_transcode_roundtrip(void)
{
    char raw_in[512], jp3d_eb[512], jp3d_ht[512], raw_out[512];
    snprintf(raw_in,  sizeof(raw_in),  "%s/cli_tc_in.raw",      temp_dir);
    snprintf(jp3d_eb, sizeof(jp3d_eb), "%s/cli_tc_ebcot.jp3d",  temp_dir);
    snprintf(jp3d_ht, sizeof(jp3d_ht), "%s/cli_tc_htj2k.jp3d",  temp_dir);
    snprintf(raw_out, sizeof(raw_out), "%s/cli_tc_out.raw",     temp_dir);

    if (!create_test_raw(raw_in, 8, 8, 4, 8, 0)) return 0;

    char cmd[2048];
    /* Encode with EBCOT */
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 8 -H 8 -D 4 -p 8",
             compress_path, raw_in, jp3d_eb);
    if (system(cmd) != 0) return 0;

    /* Transcode to HTJ2K */
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s",
             transcode_path, jp3d_eb, jp3d_ht);
    if (system(cmd) != 0) return 0;

    /* Decode the transcoded file */
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s",
             decompress_path, jp3d_ht, raw_out);
    if (system(cmd) != 0) return 0;

    return files_equal(raw_in, raw_out);
}

/** 7. --help returns 0. */
static int test_help_exit_code(void)
{
    char cmd[2048];
    int ret;

    snprintf(cmd, sizeof(cmd), "%s --help > " NULL_DEVICE, compress_path);
    ret = system(cmd);
    if (ret != 0) return 0;

    snprintf(cmd, sizeof(cmd), "%s --help > " NULL_DEVICE, decompress_path);
    ret = system(cmd);
    if (ret != 0) return 0;

    snprintf(cmd, sizeof(cmd), "%s --help > " NULL_DEVICE, dump_path);
    ret = system(cmd);
    if (ret != 0) return 0;

    snprintf(cmd, sizeof(cmd), "%s --help > " NULL_DEVICE, transcode_path);
    ret = system(cmd);
    if (ret != 0) return 0;

    return 1;
}

/** 8. --version returns 0 and contains version string. */
static int test_version_output(void)
{
    char ver_out[512];
    snprintf(ver_out, sizeof(ver_out), "%s/cli_ver.txt", temp_dir);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s --version > %s", compress_path, ver_out);
    if (system(cmd) != 0) return 0;

    FILE *fp = fopen(ver_out, "r");
    if (!fp) return 0;
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) { fclose(fp); return 0; }
    fclose(fp);

    if (!strstr(buf, OPJ_JP3D_VERSION)) return 0;
    return 1;
}

/** 9. Missing required args → non-zero exit. */
static int test_missing_args(void)
{
    char cmd[2048];

    snprintf(cmd, sizeof(cmd), "%s 2>" NULL_DEVICE, compress_path);
    if (system(cmd) == 0) return 0; /* should fail */

    snprintf(cmd, sizeof(cmd), "%s 2>" NULL_DEVICE, decompress_path);
    if (system(cmd) == 0) return 0;

    snprintf(cmd, sizeof(cmd), "%s 2>" NULL_DEVICE, dump_path);
    if (system(cmd) == 0) return 0;

    snprintf(cmd, sizeof(cmd), "%s 2>" NULL_DEVICE, transcode_path);
    if (system(cmd) == 0) return 0;

    return 1;
}

/** 10. Verbose mode doesn't crash. */
static int test_verbose_mode(void)
{
    char raw_in[512], jp3d[512], raw_out[512];
    snprintf(raw_in,  sizeof(raw_in),  "%s/cli_verb_in.raw", temp_dir);
    snprintf(jp3d,    sizeof(jp3d),    "%s/cli_verb.jp3d",   temp_dir);
    snprintf(raw_out, sizeof(raw_out), "%s/cli_verb_out.raw", temp_dir);

    if (!create_test_raw(raw_in, 4, 4, 4, 8, 0)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 4 -H 4 -D 4 -p 8 -v 2>" NULL_DEVICE,
             compress_path, raw_in, jp3d);
    if (system(cmd) != 0) return 0;

    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -v 2>" NULL_DEVICE,
             decompress_path, jp3d, raw_out);
    if (system(cmd) != 0) return 0;

    return files_equal(raw_in, raw_out);
}

/** 11. Dump shows correct volume size. */
static int test_dump_volume_size(void)
{
    char raw_in[512], jp3d[512], dsz_out[512];
    snprintf(raw_in,  sizeof(raw_in),  "%s/cli_dsz_in.raw",  temp_dir);
    snprintf(jp3d,    sizeof(jp3d),    "%s/cli_dsz.jp3d",    temp_dir);
    snprintf(dsz_out, sizeof(dsz_out), "%s/cli_dsz_out.txt", temp_dir);

    if (!create_test_raw(raw_in, 16, 8, 4, 8, 0)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 16 -H 8 -D 4 -p 8",
             compress_path, raw_in, jp3d);
    if (system(cmd) != 0) return 0;

    snprintf(cmd, sizeof(cmd),
             "%s -i %s > %s",
             dump_path, jp3d, dsz_out);
    if (system(cmd) != 0) return 0;

    FILE *fp = fopen(dsz_out, "r");
    if (!fp) return 0;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    if (!strstr(buf, "16 x 8 x 4")) return 0;
    return 1;
}

/** 12. Custom decomposition levels. */
static int test_custom_decomp(void)
{
    char raw_in[512], jp3d[512], raw_out[512];
    snprintf(raw_in,  sizeof(raw_in),  "%s/cli_cd_in.raw",  temp_dir);
    snprintf(jp3d,    sizeof(jp3d),    "%s/cli_cd.jp3d",    temp_dir);
    snprintf(raw_out, sizeof(raw_out), "%s/cli_cd_out.raw", temp_dir);

    if (!create_test_raw(raw_in, 8, 8, 8, 8, 0)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 8 -H 8 -D 8 -p 8 -n 2,2,2",
             compress_path, raw_in, jp3d);
    if (system(cmd) != 0) return 0;

    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s",
             decompress_path, jp3d, raw_out);
    if (system(cmd) != 0) return 0;

    return files_equal(raw_in, raw_out);
}

/** 13. Verbose output contains expected content (MT-CLI-011). */
static int test_verbose_content(void)
{
    char raw_in[512], jp3d[512], verb_out[512];
    snprintf(raw_in,   sizeof(raw_in),   "%s/cli_vc_in.raw",  temp_dir);
    snprintf(jp3d,     sizeof(jp3d),     "%s/cli_vc.jp3d",    temp_dir);
    snprintf(verb_out, sizeof(verb_out), "%s/cli_vc_err.txt", temp_dir);

    if (!create_test_raw(raw_in, 4, 4, 4, 8, 0)) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s -o %s -W 4 -H 4 -D 4 -p 8 -v 2>%s",
             compress_path, raw_in, jp3d, verb_out);
    if (system(cmd) != 0) return 0;

    FILE *fp = fopen(verb_out, "r");
    if (!fp) return 0;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    /* Verbose output should mention input dimensions and byte count. */
    if (!strstr(buf, "4") || !strstr(buf, "8-bit")) return 0;
    if (!strstr(buf, "Encoded")) return 0;

    return 1;
}

/** 14. Missing input file → non-zero exit and error message (MT-CLI-014). */
static int test_missing_file(void)
{
    char err_out[512];
    snprintf(err_out, sizeof(err_out), "%s/cli_mf_err.txt", temp_dir);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -i %s/nonexistent_file.raw -o %s/out.jp3d "
             "-W 4 -H 4 -D 4 -p 8 2>%s",
             compress_path, temp_dir, temp_dir, err_out);
    int ret = system(cmd);
    if (ret == 0) return 0; /* should fail */

    /* Verify error output is non-empty */
    if (!file_nonempty(err_out)) return 0;

    return 1;
}

/** 15. Decompress --version contains version string (MT-CLI-016). */
static int test_decompress_version(void)
{
    char ver_out[512];
    snprintf(ver_out, sizeof(ver_out), "%s/cli_decver.txt", temp_dir);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s --version > %s", decompress_path, ver_out);
    if (system(cmd) != 0) return 0;

    FILE *fp = fopen(ver_out, "r");
    if (!fp) return 0;
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) { fclose(fp); return 0; }
    fclose(fp);

    if (!strstr(buf, OPJ_JP3D_VERSION)) return 0;
    return 1;
}

/** 16. Dump --version contains version string (MT-CLI-017). */
static int test_dump_version(void)
{
    char ver_out[512];
    snprintf(ver_out, sizeof(ver_out), "%s/cli_dmpver.txt", temp_dir);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s --version > %s", dump_path, ver_out);
    if (system(cmd) != 0) return 0;

    FILE *fp = fopen(ver_out, "r");
    if (!fp) return 0;
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) { fclose(fp); return 0; }
    fclose(fp);

    if (!strstr(buf, OPJ_JP3D_VERSION)) return 0;
    return 1;
}

/** 17. Transcode --version contains version string (MT-CLI-018). */
static int test_transcode_version(void)
{
    char ver_out[512];
    snprintf(ver_out, sizeof(ver_out), "%s/cli_tcver.txt", temp_dir);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s --version > %s", transcode_path, ver_out);
    if (system(cmd) != 0) return 0;

    FILE *fp = fopen(ver_out, "r");
    if (!fp) return 0;
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) { fclose(fp); return 0; }
    fclose(fp);

    if (!strstr(buf, OPJ_JP3D_VERSION)) return 0;
    return 1;
}

/* ----------------------------------------------------------------------- */
/* main                                                                    */
/* ----------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    /* Determine a cross-platform temporary directory */
    const char *td = NULL;
#ifdef _WIN32
    td = getenv("TEMP");
    if (!td) td = getenv("TMP");
#else
    td = getenv("TMPDIR");
#endif
    if (!td) {
#ifdef _WIN32
        td = ".";
#else
        td = "/tmp";
#endif
    }
    snprintf(temp_dir, sizeof(temp_dir), "%s", td);

    /* Expect tool paths from environment or construct from build dir */
    const char *bindir = NULL;
    if (argc > 1) {
        bindir = argv[1];
    }

    if (bindir) {
        snprintf(compress_path,   sizeof(compress_path),   "%s/opj_jp3d_compress",   bindir);
        snprintf(decompress_path, sizeof(decompress_path), "%s/opj_jp3d_decompress", bindir);
        snprintf(dump_path,       sizeof(dump_path),       "%s/opj_jp3d_dump",       bindir);
        snprintf(transcode_path,  sizeof(transcode_path),  "%s/opj_jp3d_transcode",  bindir);
    } else {
        snprintf(compress_path,   sizeof(compress_path),   "opj_jp3d_compress");
        snprintf(decompress_path, sizeof(decompress_path), "opj_jp3d_decompress");
        snprintf(dump_path,       sizeof(dump_path),       "opj_jp3d_dump");
        snprintf(transcode_path,  sizeof(transcode_path),  "opj_jp3d_transcode");
    }

    RUN_TEST(test_roundtrip_8bit);
    RUN_TEST(test_roundtrip_16bit);
    RUN_TEST(test_roundtrip_htj2k);
    RUN_TEST(test_roundtrip_signed);
    RUN_TEST(test_dump_output);
    RUN_TEST(test_transcode_roundtrip);
    RUN_TEST(test_help_exit_code);
    RUN_TEST(test_version_output);
    RUN_TEST(test_missing_args);
    RUN_TEST(test_verbose_mode);
    RUN_TEST(test_dump_volume_size);
    RUN_TEST(test_custom_decomp);
    RUN_TEST(test_verbose_content);
    RUN_TEST(test_missing_file);
    RUN_TEST(test_decompress_version);
    RUN_TEST(test_dump_version);
    RUN_TEST(test_transcode_version);

    printf("Passed %d/%d tests\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
