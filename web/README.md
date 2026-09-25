# qjs-nanovg in the browser

NanoVG compiled to WebAssembly with Emscripten, drawing on WebGL2, behind a JavaScript module that mirrors the native `nanovg` QuickJS module (`nvgjs-module.c`). Scripts written for `qjsm` run in the browser with the same `import ... from 'nanovg'`.

Live demo: [transistorisiert.ch/nanovg](https://transistorisiert.ch/nanovg/)

## Layout

| File | Role |
|---|---|
| `nanovg-web.c` | C side. Thin exports (`nvgw_*`) over NanoVG's GLES3 backend. Every function takes the `NVGcontext*`, so several contexts can coexist. |
| `nanovg.js` | The `nanovg` module: the full native API surface on top of the wasm exports. |
| `nanovg-web.js`, `nanovg-web.wasm` | Emscripten output (committed, rebuilt by `build.sh` / CMake). |
| `build.sh` | The `emcc` command line, the single source of truth for build flags. |
| `glfw.js`, `std.js` | Browser stand-ins for the `glfw` and `std` modules the demos import. |
| `asyncify.js`, `run.js`, `run.html` | Loader that runs an unmodified example script in a page. |
| `index.html` | Landing page with all three demos. |
| `compat*.js`, `test-api.*` | Compatibility checks against the native module (see Testing). |
| `deploy.sh` | Copies the files a web server needs into a directory. |
| `acorn.mjs`, `fonts/DejaVuSans.ttf` | Vendored parser used by `asyncify.js`; font the demos load. |

## Building

The web build is part of a normal CMake build when `emcc` is found:

```sh
cmake -B build && cmake --build build          # also builds web/nanovg-web.{js,wasm}
cmake -B build -DEMCC=/path/to/emcc            # point at a specific emcc
cmake -B build -DBUILD_WEB=OFF                 # skip it
```

`emcc` is looked up on `PATH`, then under `$EMSDK/upstream/emscripten` and `$EMSCRIPTEN`. Without one, CMake prints a status message and builds everything else. The output is written into this directory, so `git status` shows the two artifacts as modified after a rebuild.

Without CMake:

```sh
EMCC=/path/to/emcc sh web/build.sh
```

The build uses `-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sMODULARIZE -sEXPORT_ES6 -sALLOW_MEMORY_GROWTH -sFORCE_FILESYSTEM=1`. `-sGROWABLE_ARRAYBUFFERS=0` is needed because Chrome's `TextDecoder` rejects the resizable buffers Emscripten 6 uses by default.

## Running

`run.html` loads `./examples/<name>.js`, so serve a directory that has the examples next to the page. `deploy.sh` builds that layout:

```sh
sh web/deploy.sh /tmp/nanovg-site
cd /tmp/nanovg-site && python3 -m http.server 8000
```

| URL | Shows |
|---|---|
| `/index.html#planets` | Landing page, planets tab (also `#curve-editor`, `#polyline-editor`) |
| `/run.html?example=curve-editor` | One example on its own page |

`test-api.html` can be served straight from `web/`.

Use a normal, visible tab. Browsers pause `requestAnimationFrame` in background tabs, which freezes the demos.

## How it works

### Drawing
`nanovg.js` awaits `createModule()` at import time. `CreateGL3(flags)` creates one WebGL2 context on the `#canvas` element (stencil on, MSAA off, because NanoVG does its own antialiasing) and then a `NVGcontext` per call. Later calls reuse the WebGL context. Call it after the canvas has its final size, as no viewport is set later (same as native).

### API layer
Context methods are generated from a table in `nanovg.js`: declared length, argument kinds, arguments required. The generated methods:

- throw `InternalError: need N arguments` where the native code does
- convert arguments as the C code does (`+x` for floats, `x | 0` for ints, strings copied into wasm memory, colors read from arrays)
- report the same `length`, `name` and property flags, so the API surface matches the native module exactly

`Color`, `Transform` and `Paint` are classes (`Color` and `Transform` extend `Float32Array`). `Transform` follows the native argument protocol, including the optional in/out matrix first argument. Optional text `end` arguments count code points, as native does.

### Files
Browsers cannot read files synchronously, so `CreateFont(name, path)` and `CreateImage(path, flags)` read from Emscripten's in-memory file system through the real `nvgCreateFont` / `nvgCreateImage`. Decoding and failure results (`-1` for fonts, `0` for images) therefore match native.

Put bytes there with the one extra export:

```js
import { writeFile, CreateGL3 } from 'nanovg';

writeFile('/fonts/DejaVuSans.ttf', await (await fetch('DejaVuSans.ttf')).arrayBuffer());
const nvg = CreateGL3(0);
nvg.CreateFont('sans', '/fonts/DejaVuSans.ttf');
```

`run.js` does this automatically: it collects the string literals of the example that end in `.ttf` or an image extension, fetches them (fonts from `./fonts/<basename>`, images from `./<path>`) and writes them at the exact path the script uses.

### Running the demos unchanged
The demos loop with a blocking `while(...) { ...; glfw.poll(); }`. A browser only shows a frame when the script yields, so `asyncify.js` rewrites the source before it runs: every function that calls `poll()`, directly or through other functions, is matched by name and made `async`, and each call to it gets an `await`. `poll()` in `glfw.js` resolves on the next animation frame.

### Shims
| Module | Provided |
|---|---|
| `glfw` | `Window` (`hint`, `size`, `shouldClose`, `swapBuffers`, `destroy`, the `handleCursorPos`, `handleMouseButton`, `handleKey` and `handleCharMods` callbacks), `context.current`, `poll()`, key and window-hint constants |
| `std` | `open(path, 'w')` with `puts` and `close`; closing downloads the file |

Mouse and keyboard only. A cursor position is reported before every button event, as GLFW does.

## Differences from the native module

- `CreateFont`/`CreateImage` need their files in the virtual file system first (see Files).
- `Color`, `Paint`, `Context` are constructible and `CreateImageMem`/`CreateImageRGBA` also accept typed arrays. The native objects are plain, non-callable objects.
- `writeFile` is web-only.
- There is no `CreateGL2`; the native GL3 build does not export it either.
- `ReadPixels` reads the bound framebuffer. On the default framebuffer, call it in the same task as the drawing, before yielding.
- `BeginFrame` does not clear or set the viewport, matching native.
- Float math is done in doubles and stored as float32, so results can differ from the C code in the last bit.

## Testing

**API surface and pure math**, native against web (needs `qjsm`, `node`, and a built `nanovg.so`):

```sh
qjsm web/compat-native.js /path/to/nanovg.so > native.json
node web/compat-node.js > web.json
node web/compat-diff.js native.json web.json     # prints "identical" or the differences
```

The diff covers export names and values, every function's `length`, `name` and property flags, class tags, transform and color results, and error messages. `writeFile` is the only allowed extra.

**Rendering and everything that needs GL**: open `test-api.html` in Chrome. It calls every context method against a real WebGL2 context, checks pixels read back from the default framebuffer and from a framebuffer object, and sets the page title to `PASS <n>` or `FAIL <n>` with the details in the page.

## Deploying

`deploy.sh` copies the site into `/var/www/html/nanovg` by default (or the directory you pass). nginx needs two MIME types that `mime.types` lacks:

```nginx
location = /nanovg { return 301 $scheme://$host/nanovg/; }
location ^~ /nanovg/ {
  include mime.types;
  types {
    application/wasm wasm;
    application/javascript mjs;
  }
  index index.html;
  add_header Cache-Control "no-cache";
}
```

Without the `wasm` type the module still loads through Emscripten's fallback, but with a console warning. Without the `mjs` type the browser refuses `acorn.mjs`.

Not deployed: any example images (the demos do not draw them; `planets.js` still requests two and gets a 404).

## Limitations

- Touch input is not handled.
- `asyncify.js` matches functions by name, so two unrelated functions with the same name where only one calls `poll()` would both get an `await`.
- Esc and Q end an example's loop; reload the page to restart.
