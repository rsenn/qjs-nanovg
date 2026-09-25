# qjs-nanovg

[QuickJS](https://bellard.org/quickjs/) bindings for [NanoVG](https://github.com/memononen/nanovg), a small antialiased 2D vector graphics library modelled after the HTML5 Canvas API and rendered with OpenGL.

The bindings are a native module. It builds to a shared library, `nanovg.so`, that `qjsm` (the module-enabled QuickJS) loads on `import ... from 'nanovg'`:

```js
import { CreateGL3, RGBA, ANTIALIAS, STENCIL_STROKES } from 'nanovg';
```

The module only draws. The OpenGL context and window come from a separate module, `glfw`.

## Requirements

| Needed | For |
|---|---|
| QuickJS with `qjsm` and its development headers | building and running the module |
| GLEW, OpenGL | linking `nanovg.so` |
| CMake 3.5+ | building |
| The `glfw` QuickJS module | opening a window and getting a GL context |
| The `nanovg/` submodule (a fork of NanoVG) | `git clone --recursive`, or `git submodule update --init` |

## Build and install

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --install build      # may need sudo
```

`cmake --install` copies `nanovg.so` and `lib/canvas2d.js` into QuickJS's C-module directory, which CMake prints during configure (`C module directory: ...`). Override the QuickJS location with `-DQUICKJS_PREFIX=...`.

| Option | Default | Effect |
|---|---|---|
| `BUILD_STATIC_MODULES` | ON | Also builds `libnanovg.a` (`qjs-nanovg-static`), for linking nanovg into your own interpreter, for example through qjsm's `EXTERNAL_MODULES`. |
| `BUILD_WEB` | ON | Also builds the browser version in `web/` when `emcc` is found. See [web/README.md](web/README.md). |
| `BUILD_EXAMPLE` | OFF | Builds the upstream C example, `nanovg_example`, and a `run_example` target. |

If a `markdown` executable is found, the pages in `doc/` are also converted to HTML and installed.

## Usage

The API uses PascalCase names. A context is created with the free function `CreateGL3(flags)`, not with `new`:

```js
import * as glfw from 'glfw';
import { CreateGL3, DeleteGL3, STENCIL_STROKES, ANTIALIAS, RGB, RGBA } from 'nanovg';

glfw.Window.hint(glfw.CONTEXT_VERSION_MAJOR, 3);
glfw.Window.hint(glfw.CONTEXT_VERSION_MINOR, 2);
glfw.Window.hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE);
glfw.Window.hint(glfw.OPENGL_FORWARD_COMPAT, true);

const win = (glfw.context.current = new glfw.Window(1024, 768, 'NanoVG'));
const { width, height } = win.size;

const nvg = CreateGL3(STENCIL_STROKES | ANTIALIAS);

while (!win.shouldClose) {
  nvg.BeginFrame(width, height, 1);

  nvg.BeginPath();
  nvg.Rect(100, 100, 200, 150);
  nvg.FillColor(RGBA(255, 128, 0, 255));
  nvg.Fill();
  nvg.StrokeColor(RGB(255, 255, 255));
  nvg.StrokeWidth(3);
  nvg.Stroke();

  nvg.EndFrame();
  win.swapBuffers();
  glfw.poll();
}

DeleteGL3(nvg);
```

Run it with `qjsm script.js`, not plain `qjs`: `qjs` lacks the globals these scripts use and swallows uncaught errors in module mode.

## API

The complete reference, generated from the exports in `nvgjs-module.c`, is [doc/api-documentation.md](doc/api-documentation.md). In short:

| Area | Where |
|---|---|
| `CreateGL3`, `DeleteGL3`, framebuffers, `ReadPixels`, image handle interop | free functions |
| `RGB`, `RGBA`, `RGBf`, `RGBAf`, `LerpRGBA`, `TransRGBA`, `HSL`, `HSLA`, `DegToRad`, `RadToDeg` | free functions; colors are `Float32Array`-backed `Color` values |
| Frame, state, paths, fill and stroke, gradients and paints, scissor, transforms, images, fonts and text | methods on the context returned by `CreateGL3` |
| `Transform` | static helpers and chainable instance methods on a 6-float `Float32Array` |
| Constants such as `ANTIALIAS`, `ALIGN_LEFT`, `CCW`, `IMAGE_REPEATX` | module exports, without the `NVG_` prefix |

Vector arguments are flexible: a `Float32Array`, a plain array, an iterable, or separate scalar arguments are all accepted where a vector is expected.

## Examples

| File | What it is |
|---|---|
| `examples/planets.js` | Animated orbits with transforms, save/restore and image patterns. The quickest check that the module works. |
| `examples/curve-editor.js` | Interactive editor for lines, quadratic and cubic Béziers, with text buttons. |
| `examples/polyline-editor.js` | Interactive polyline editor. |
| `examples/forex.js` | Candlestick chart driven by a simulated price feed. |
| `examples/emerald-run/` | A small tile-based dungeon crawler built on `engine2d.js` (`qjsm --std emerald.js`). |

```sh
qjsm examples/planets.js
```

`lib/canvas2d.js` is a thin `CanvasRenderingContext2D` emulation on top of `nanovg`, `glfw` and `dom`, so scripts written for the browser canvas can run under `qjsm`. Its header lists the known gaps.

## Browser version

The same API also runs in the browser: NanoVG compiled to WebAssembly with Emscripten, drawing on WebGL2. It has its own documentation and a live demo, see [web/README.md](web/README.md).

## Source layout

| File | Role |
|---|---|
| `nvgjs-module.c`, `nvgjs-module.h` | The whole binding: classes, methods, free functions, constants, module init. |
| `nvgjs-utils.c`, `nvgjs-utils.h` | Argument marshalling helpers (vector or scalar arguments, typed-array output). |
| `nanovg/` | The NanoVG fork (submodule); its `nanovg.c` is compiled in. |
| `lib/canvas2d.js` | Canvas2D emulation, installed next to `nanovg.so`. |
| `examples/` | Runnable scripts. |
| `doc/` | API reference. |
| `web/` | Browser build. |
| `BUGS`, `TODO.md` | Known bugs; NanoVG functions not yet bound. |

C code follows the repo's `.clang-format`.

## License

MIT, see [LICENSE](LICENSE). NanoVG itself is under the zlib license, see `nanovg/LICENSE.txt`.
