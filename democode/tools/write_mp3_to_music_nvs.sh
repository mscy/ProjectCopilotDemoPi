#!/usr/bin/env bash
set -euo pipefail

# Generate and flash raw music_nvs partition image with one MP3 payload.
# Usage:
#   tools/write_mp3_to_music_nvs.sh <path_to_mp3> [port]

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <path_to_mp3> [port]"
  exit 1
fi

MP3_PATH="$1"
PORT="${2:-/dev/tty.usbmodem101}"

if [[ ! -f "$MP3_PATH" ]]; then
  echo "MP3 file not found: $MP3_PATH"
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PART_CSV="$PROJECT_ROOT/partitions.csv"

if [[ ! -f "$PART_CSV" ]]; then
  echo "partitions.csv not found at $PART_CSV"
  exit 3
fi

MUSIC_LINE="$(awk -F',' '
  $0 !~ /^[[:space:]]*#/ && $1 ~ /^[[:space:]]*music_nvs[[:space:]]*$/ {
    gsub(/[[:space:]]/, "", $4);
    gsub(/[[:space:]]/, "", $5);
    print $4 "," $5;
    exit
  }
' "$PART_CSV")"

if [[ -z "$MUSIC_LINE" ]]; then
  echo "music_nvs partition not found in partitions.csv"
  exit 4
fi

MUSIC_OFFSET_HEX="${MUSIC_LINE%%,*}"
MUSIC_SIZE_HEX="${MUSIC_LINE##*,}"
MUSIC_SIZE_DEC="$((MUSIC_SIZE_HEX))"

MP3_SIZE="$(wc -c < "$MP3_PATH" | tr -d ' ')"
# Keep safety headroom for NVS metadata/pages.
MAX_PAYLOAD="$((MUSIC_SIZE_DEC - 65536))"
if (( MP3_SIZE > MAX_PAYLOAD )); then
  echo "MP3 too large for music_nvs payload budget"
  echo "  mp3 bytes:       $MP3_SIZE"
  echo "  partition bytes: $MUSIC_SIZE_DEC"
  echo "  max payload:     $MAX_PAYLOAD"
  exit 5
fi

IDF_EXPORT="/Users/cuiy/esp/v5.5.2/esp-idf/export.sh"
if [[ ! -f "$IDF_EXPORT" ]]; then
  echo "ESP-IDF export script not found: $IDF_EXPORT"
  exit 6
fi

TMP_DIR="$PROJECT_ROOT/build/music_nvs_gen"
mkdir -p "$TMP_DIR"
CSV_FILE="$TMP_DIR/music_nvs.csv"
BIN_FILE="$TMP_DIR/music_nvs.bin"

rm -f "$CSV_FILE"

# shellcheck disable=SC1090
. "$IDF_EXPORT" >/dev/null 2>&1

python - "$MP3_PATH" "$BIN_FILE" <<'PY'
import os
import struct
import sys

src, dst = sys.argv[1], sys.argv[2]
payload = open(src, 'rb').read()
header = struct.pack('<IIII', 0x33504D43, len(payload), 0, 0)
with open(dst, 'wb') as out:
  out.write(header)
  out.write(payload)
PY

echo "Generated: $BIN_FILE"
echo "Flashing music_nvs at offset $MUSIC_OFFSET_HEX via $PORT"

PIDS=$(lsof -t "$PORT" 2>/dev/null || true)
if [[ -n "$PIDS" ]]; then
  kill -9 $PIDS 2>/dev/null || true
fi

python -m esptool \
  --chip esp32s3 \
  -p "$PORT" \
  -b 460800 \
  --before default_reset \
  --after hard_reset \
  write_flash "$MUSIC_OFFSET_HEX" "$BIN_FILE"

echo "Done. MP3 stored in raw partition $MUSIC_OFFSET_HEX"
