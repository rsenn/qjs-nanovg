#!/bin/sh
cd "$(dirname "$0")" && "${EMCC:-emcc}" nanovg-web.c ../nanovg/src/nanovg.c -I ../nanovg/src \
  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sMODULARIZE -sEXPORT_ES6 -sALLOW_MEMORY_GROWTH -sGROWABLE_ARRAYBUFFERS=0 -sFORCE_FILESYSTEM=1 \
  -sEXPORTED_FUNCTIONS=_malloc,_free -sEXPORTED_RUNTIME_METHODS=HEAPU8,FS,stringToUTF8,lengthBytesUTF8 \
  -O2 -o nanovg-web.js
