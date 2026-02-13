#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "Usage: $0 <binary_path> <output_png_path> [startup_seconds]" >&2
    exit 1
fi

BINARY_PATH="$1"
OUTPUT_PATH="$2"
STARTUP_SECONDS="${3:-2}"

if [[ ! -x "$BINARY_PATH" ]]; then
    echo "Binary is not executable: $BINARY_PATH" >&2
    exit 1
fi

OUTPUT_DIR="$(dirname "$OUTPUT_PATH")"
mkdir -p "$OUTPUT_DIR"

PROCESS_NAME="$(basename "$BINARY_PATH")"
LOG_PATH="$OUTPUT_DIR/${PROCESS_NAME}.capture.log"

"$BINARY_PATH" >"$LOG_PATH" 2>&1 &
APP_PID=$!

cleanup()
{
    kill "$APP_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep "$STARTUP_SECONDS"

WINDOW_ID="$(
    PROC_NAME="$PROCESS_NAME" swift -e '
import CoreGraphics
import Foundation

let processName = ProcessInfo.processInfo.environment["PROC_NAME"] ?? ""
let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly], kCGNullWindowID) as? [[String: Any]] ?? []
let target = windows.first
{
    window in
    let owner = window[kCGWindowOwnerName as String] as? String
    let layer = window[kCGWindowLayer as String] as? Int ?? -1
    return owner == processName && layer == 0
}

if let id = target?[kCGWindowNumber as String] as? Int
{
    print(id)
}
'
)"

if [[ -z "$WINDOW_ID" ]]; then
    echo "Could not find on-screen window id for process: $PROCESS_NAME" >&2
    exit 1
fi

screencapture -x -o -l "$WINDOW_ID" "$OUTPUT_PATH"
echo "Captured $PROCESS_NAME window ($WINDOW_ID) -> $OUTPUT_PATH"
