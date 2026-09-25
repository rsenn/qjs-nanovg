# TODO

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
