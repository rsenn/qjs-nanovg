# Build options

Configure with `cmake -B build -D<OPTION>=<value>`.

| Option | Default | Effect |
|---|---|---|
| `BUILD_SHARED` | ON | Builds `nanovg.so` (`qjs-nanovg`). From `quickjs_module_options()`. |
| `BUILD_STATIC` | ON | Builds `libnanovg.a` and `quickjs-nanovg.module.cmake` (`qjs-nanovg-static`). From `quickjs_module_options()`. |
| `BUILD_WEB` | ON | Builds `web/nanovg-web.js` and `web/nanovg-web.wasm` with Emscripten (target `qjs-nanovg-web`, part of `ALL`). |
| `BUILD_EXAMPLE` | OFF | Builds the upstream C example `nanovg_example` and a `run_example` target. |

## BUILD_WEB

`emcc` is looked up on `PATH`, then under `$EMSDK/upstream/emscripten` and
`$EMSCRIPTEN`. Use `-DEMCC=/path/to/emcc` to choose one. If none is found,
CMake prints a status message and the rest still builds. The outputs are
written to `web/` and are git-ignored.

If `emcc` stops with `cache directory ... is not writable`, set
`EM_CACHE` to a writable directory. The first build into an empty cache
compiles Emscripten's system libraries and takes over 10 minutes.

Verified 2026-11-20 with `-DBUILD_WEB=ON`: configure succeeds and the build
produces both files (93 KB and 195 KB). They were not yet run in a browser.
See `web/README.md` for the browser side.
