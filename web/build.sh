#!/bin/bash
# Build the Apple 1 Emulator WebAssembly version
# Requires: emsdk (https://emscripten.org/docs/getting_started/downloads.html)
#
# Usage:
#   source ~/emsdk/emsdk_env.sh
#   ./build.sh

set -e

SRCDIR="../src"

emcc \
  wasm_bridge.c wasm_stubs.c \
  "$SRCDIR/cpu.c" "$SRCDIR/memory.c" "$SRCDIR/pia.c" "$SRCDIR/basic.c" \
  "$SRCDIR/roms_builtin.c" "$SRCDIR/cpu_bridge.c" \
  -O2 -s WASM=1 --no-entry \
  -s "EXPORTED_FUNCTIONS=['_wasm_init','_wasm_step_frame','_wasm_send_key','_wasm_reset','_wasm_get_screen_char','_wasm_get_cursor_col','_wasm_get_cursor_row','_wasm_load_program','_wasm_send_break','_malloc','_free']" \
  -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8','lengthBytesUTF8']" \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s MODULARIZE=1 -s EXPORT_NAME='Apple1Module' \
  -I"$SRCDIR" \
  -o apple1.js

echo "Build complete: apple1.js + apple1.wasm"
echo "Serve this directory with any HTTP server to run:"
echo "  python3 -m http.server 8080"
