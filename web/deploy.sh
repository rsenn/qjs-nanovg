#!/bin/sh
# Copies just what the site needs into a directory a web server can serve.
set -e
src="$(cd "$(dirname "$0")" && pwd)"
dest="${1:-/var/www/html/nanovg}"

mkdir -p "$dest/examples" "$dest/fonts"
cd "$src"
cp index.html run.html run.js asyncify.js acorn.mjs glfw.js std.js nanovg.js nanovg-web.js nanovg-web.wasm "$dest/"
cp fonts/DejaVuSans.ttf "$dest/fonts/"
cp ../examples/planets.js ../examples/curve-editor.js ../examples/polyline-editor.js "$dest/examples/"
