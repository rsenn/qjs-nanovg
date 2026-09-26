# TODO

## Compatibility assessment: nanovg.js vs qjs-nanovg (2026-09-26)

Ran `compat-native.js` (qjsm, `build/x86_64-linux-gnu/nanovg.so`, built 2026-09-25 from the current
`nvgjs-module.c`) and `compat-node.js` (Node 23, `nanovg.js`) through `compat-diff.js`: **identical**.

| Area                                             | Native        | Web           | Status                                   |
| ------------------------------------------------ | ------------- | ------------- | ---------------------------------------- |
| Module exports (51 constants, 20 functions, 4 classes) | 75        | 75 + `writeFile` | same, `writeFile` is web-only         |
| `Context.prototype` methods (names, length, flags) | 67          | 67            | same                                     |
| `Transform` statics / prototype                  | 9 / 21        | 9 / 21        | same                                     |
| `Color` prototype, `Symbol.toStringTag` values   | 4             | 4             | same                                     |
| Color, angle and `Transform` results (float32)   | -             | -             | same within 1e-5                         |
| Error messages of our own argument checks        | -             | -             | same (13 cases)                          |
| Rendering, text metrics, image ids               | GL3 backend   | GLES3 backend | not compared (see Unverified)            |

- Not bound in either build (unchanged scripts fail the same way): the functions listed in `../TODO.md`
  (fallback fonts, `CreateFontMem*`, `FontFaceId`, `GlobalComposite*`, `TextBreakLines`,
  `TextGlyphPositions`, `TextMetrics`, ...).
- The harness only exercises code that needs no GL context, so "identical" covers the API shape and
  the pure math, not drawing.
- Unchanged scripts also run natively: `browser-emu.js` plus the `web/nanovg.js` alias in
  `package.json` run `web/examples/{planets,forex,curve-editor,polyline-editor}.js` under qjsm
  (rendering and mouse input checked by hand on 2026-09-26; keyboard only checked at event level).
- The installed `/usr/local/lib/x86_64-linux-gnu/quickjs/nanovg.so` is dated 2026-09-15, older than
  `nvgjs-module.c`; run the harness against a fresh build, not the installed one.
- `package.json` (added for the alias) has no `"type"`, so Node prints `MODULE_TYPELESS_PACKAGE_JSON`
  when running `compat-node.js`. Harmless; left alone, and I did not test whether `"type": "module"`
  would break the emscripten glue (`nanovg-web.js` calls `require` in its Node branch).

## Gaps to full nvgjs-module.c compatibility

The exported API surface, argument protocols, error messages for our own checks and the
pure-math results (Transform, colors, angles) are identical to the native module (`compat*.js`).
What has not been verified or is deliberately different:

### Unverified

- Rendering parity: the web build uses the GLES3 backend, the native module the GL3 backend with
  `NANOVG_GL_USE_UNIFORMBUFFER`, so they run different shader paths. Pixels were never compared.
  Idea: define `NANOVG_GL_USE_UNIFORMBUFFER` for GLES3 in `nanovg-web.c` (WebGL2 has UBOs) and
  compare renders of the same scene.
- GL-dependent results (`Text` advance, `TextBounds*`, `ImageSize`, `CurrentTransform`, font and image
  ids, `ReadPixels` contents) were only checked for sanity in `test-api.html`, never against
  native values. Native needs a GL context (no `DISPLAY` here); run it under xvfb or a hidden glfw
  window and diff the numbers.

### Different on purpose

- `Color`, `Paint` and `Context` are constructible in the web build (plain non-callable objects
  natively); `CreateImageMem`/`CreateImageRGBA` also accept typed arrays; `writeFile` is web-only.
- `StrokePaint` with a non-paint throws a `TypeError`; natively it returns an exception without a
  message (`JS_GetOpaque` instead of `JS_GetOpaque2` in `nvgjs_Context_StrokePaint`).
- Float math runs in doubles and is stored as float32, so the last bit can differ from C.
- Error messages that come from the JS engine itself (V8 vs QuickJS) are not matched.

### Missing environment for unchanged scripts

- `CreateFont`/`CreateImage` only find files that were written to the virtual FS first; `run.js`
  finds them by scanning string literals, so paths built at runtime are missed.
- `glfw` shim: no `handleSize`/resize events, no joystick, no touch; `std` shim only has `open(path, 'w')`.
  Scripts using other modules (`os`, `dom`, ... as `../lib/canvas2d.js` does) do not run.
- Blocking `glfw.poll()` loops only work through the by-name rewrite in `asyncify.js`.
- `ReadPixels` on the default framebuffer only returns the frame in the same task as the drawing.
