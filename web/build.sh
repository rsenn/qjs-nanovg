#!/bin/sh
# Builds nanovg-web.js + nanovg-web.wasm. Usage: build.sh [outdir]   (EMCC=... to pick the compiler)
cd "$(dirname "$0")" || exit 1
EMCC=${EMCC:-emcc}
OUT=${1:-.}

# Emscripten 6 defaults to resizable ArrayBuffers, which Chrome's TextDecoder rejects; older
# releases don't know the setting (and don't need it).
EXTRA=
if "$EMCC" -sGROWABLE_ARRAYBUFFERS=0 -x c -E /dev/null -o /dev/null 2>/dev/null; then
  EXTRA=-sGROWABLE_ARRAYBUFFERS=0
fi

exec "$EMCC" nanovg-web.c ../nanovg/src/nanovg.c -I ../nanovg/src \
  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sMODULARIZE -sEXPORT_ES6 -sALLOW_MEMORY_GROWTH $EXTRA -sFORCE_FILESYSTEM=1 \
  -sEXPORTED_FUNCTIONS=_malloc,_free -sEXPORTED_RUNTIME_METHODS=HEAPU8,FS,stringToUTF8,lengthBytesUTF8 \
  -O2 -o "$OUT/nanovg-web.js"
