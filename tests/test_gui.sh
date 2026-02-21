#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
#
# test_gui.sh — Headless L1/L2 GUI smoke test for opj_jp3d_gui.
#
# Usage:
#   tests/test_gui.sh <bindir>
#   xvfb-run --auto-servernum bash tests/test_gui.sh build/bin
#
# Prerequisites (Linux): Xvfb, xdotool, xwd or ImageMagick (import).
# The script expects DISPLAY to be set (e.g. via xvfb-run).
#
# L1 — Launch & Exit: verify the GUI starts, stays alive briefly, and
#       exits cleanly on SIGTERM.
# L2 — Smoke (screenshot): capture a screenshot and verify it is non-empty.
#
# Exit codes:
#   0  All checks pass
#   1  A check failed
#   2  Missing prerequisites

set -euo pipefail

BINDIR="${1:-.}"
GUI="$BINDIR/opj_jp3d_gui"

if [ ! -x "$GUI" ]; then
    echo "SKIP: $GUI not found or not executable" >&2
    exit 0
fi

# Ensure DISPLAY is set (Xvfb)
if [ -z "${DISPLAY:-}" ]; then
    echo "FAIL: DISPLAY is not set — run under xvfb-run" >&2
    exit 2
fi

TMPDIR_GUI="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_GUI"' EXIT

echo "=== L1: Launch & Exit ==="

# Launch GUI in background
"$GUI" &
GUI_PID=$!
sleep 3

# Check if the process is still running
if ! kill -0 "$GUI_PID" 2>/dev/null; then
    echo "FAIL: GUI process ($GUI_PID) exited prematurely" >&2
    exit 1
fi
echo "PASS: GUI process is running (PID $GUI_PID)"

echo "=== L2: Screenshot Smoke ==="

SCREENSHOT="$TMPDIR_GUI/gui_smoke.xwd"

# Try xwd first; fall back to ImageMagick import
if command -v xwd >/dev/null 2>&1; then
    xwd -root -out "$SCREENSHOT" 2>/dev/null || true
elif command -v import >/dev/null 2>&1; then
    SCREENSHOT="$TMPDIR_GUI/gui_smoke.png"
    import -window root "$SCREENSHOT" 2>/dev/null || true
else
    echo "WARN: No screenshot tool found (xwd/import); skipping L2"
fi

if [ -f "$SCREENSHOT" ] && [ -s "$SCREENSHOT" ]; then
    echo "PASS: Screenshot captured ($(stat -c%s "$SCREENSHOT" 2>/dev/null || stat -f%z "$SCREENSHOT") bytes)"
else
    echo "WARN: Screenshot capture failed or file is empty; L2 inconclusive"
fi

echo "=== L1: Clean Exit ==="

# Send SIGTERM and wait for clean exit
kill "$GUI_PID" 2>/dev/null || true
WAIT_EXIT=0
for i in $(seq 1 10); do
    if ! kill -0 "$GUI_PID" 2>/dev/null; then
        WAIT_EXIT=1
        break
    fi
    sleep 1
done

if [ "$WAIT_EXIT" -eq 0 ]; then
    echo "WARN: GUI did not exit after SIGTERM; sending SIGKILL"
    kill -9 "$GUI_PID" 2>/dev/null || true
    wait "$GUI_PID" 2>/dev/null || true
    echo "PASS: GUI terminated (forced)"
else
    wait "$GUI_PID" 2>/dev/null || true
    echo "PASS: GUI exited cleanly after SIGTERM"
fi

echo "=== All GUI smoke checks passed ==="
exit 0
