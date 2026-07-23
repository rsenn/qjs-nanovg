# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A **QuickJS native C module** that exposes [NanoVG](https://github.com/memononen/nanovg)
(antialiased 2D vector graphics over OpenGL) to JavaScript. It builds to a shared library
`nanovg.so` that `qjsm` loads when JS code does `import ... from 'nanovg'`.

The OpenGL context/window comes from a *separate* QuickJS module (`glfw`); this module only
links GLEW + OpenGL (note `GLFW_LIBRARY` is intentionally commented out in `CMakeLists.txt`).
The `nanovg/` directory is a git submodule (forked NanoVG); its `nanovg.c` is compiled in.

## Build & run

```sh
# Configure + build (out-of-tree). Needs QuickJS dev headers, GLEW, OpenGL.
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Install nanovg.so into QuickJS's C-module dir (default /usr/local/lib/<arch>/quickjs/)
cmake --install build      # may need sudo

# Run a script — qjsm is the module-enabled QuickJS binary (NOT plain qjs)
qjsm test-nanovg.js <title>
```

- `-DBUILD_EXAMPLE=ON` additionally builds the upstream C `nanovg_example` and a
  `run_example` target.
- The CMake module dir auto-derives an arch suffix (e.g. `lib/x86_64-linux-gnu/quickjs`) via
  `cmake/UseMultiArch.cmake`. `FindQuickJS.cmake` locates QuickJS by searching common prefixes;
  override with `-DQUICKJS_PREFIX=...` if not found.
- There is no test framework. `test-nanovg.js` is a runnable demo (opens a GLFW window); it is
  the de-facto integration check.

## Source layout

- `nvgjs-module.c` — the entire binding: all class definitions, methods, free functions,
  constants, and module init. This is where ~all changes go.
- `nvgjs-utils.c` / `nvgjs-utils.h` — argument marshalling helpers used throughout the module
  (see below). Keep generic JS↔C conversion logic here, not in the module.

## How the bindings are structured

Everything is generated through macros in `nvgjs-module.h`:

- `NVGJS_DECL(Class, Fn)` declares a `JSValue nvgjs_Class_Fn(ctx, this, argc, argv, magic)`.
  `Class` is `Context` for `NVGContext` methods or `func` for free functions.
- `NVGJS_METHOD(Class, Fn, len)` / `NVGJS_FUNC(Fn, len)` register an entry in a
  `JSCFunctionListEntry[]` table. The JS-visible name is exactly `Fn`.
- `NVGJS_FLAG(name)` / `NVGJS_CONST(name)` export `NVG_##name` as the JS property `name`.
- `NVGJS_CONTEXT(obj)` / `NVGJS_FRAMEBUFFER(obj)` unwrap the native pointer from a JS object's
  opaque slot (returns `JS_EXCEPTION` on type mismatch).

To add a binding: write an `NVGJS_DECL`, implement it, then add it to the matching table
(`nvgjs_context_methods[]`, `nvgjs_funcs[]`, etc.). Free-function exports also need a
`JS_AddModuleExport`-equivalent — they're added en masse via `JS_AddModuleExportList` from
`nvgjs_funcs`, but named class exports (`Context`, `Color`, `Transform`, `Paint`) are listed
explicitly in both `nvgjs_init` and `nvgjs_init_module`.

### JS classes / values

- **Context** (class id `nvgjs_context_class_id`): wraps `NVGcontext*` in the opaque slot.
  **Not constructed with `new`** — create it via the free function `CreateGL3(flags)`
  (or `CreateGL2`), which runs `glewInit()` then `nvgCreateGL3`. `DeleteGL3(ctx)` frees it.
  The finalizer is a known no-op (`// XXX: bug`), so contexts leak on GC.
- **Color** / **Transform**: backed by `Float32Array` (their prototypes extend the
  `Float32Array` prototype). Color exposes `r/g/b/a` getters/setters; Transform is 6 floats
  with static helpers (`Transform.Translate`, `.Scale`, `.Rotate`, `.Multiply`, ...).
- **Paint** (`nvgjs_paint_class_id`): opaque `NVGpaint`, returned by gradient/imagePattern
  methods. **Framebuffer** is wired up but its module export is commented out.

### Argument conventions (`nvgjs-utils.c`)

Vector arguments are flexible: `nvgjs_inputarray` / `nvgjs_input` accept a `Float32Array`
(zero-copy), a plain `Array`, an iterable, or (with a `prop_map`) an object with named keys.
Outputs go through `nvgjs_outputarray` (requires a `Float32Array`) or `nvgjs_copyarray` /
`nvgjs_copyobject`. Use `nvgjs_arguments` for the "either an array/vector or N scalar args"
pattern. Prefer these helpers over hand-rolling `JS_To*` loops.

## API naming — IMPORTANT

The actual JS API uses **PascalCase** method and function names and creates the context via a
free function:

```js
import { CreateGL3, STENCIL_STROKES, ANTIALIAS, RGB, RGBA } from 'nanovg';
const nvg = CreateGL3(STENCIL_STROKES | ANTIALIAS);
nvg.BeginFrame(w, h, 1); nvg.BeginPath(); nvg.Rect(...); nvg.Fill(); nvg.EndFrame();
```

`README.md` documents a *different* lowercase/`new NVGContext()` API that does **not** match
the current code. Treat `test-nanovg.js` and the `nvgjs_*_methods[]` / `nvgjs_funcs[]` tables
in `nvgjs-module.c` as the source of truth for names, not the README.

## Formatting

C code is formatted with `clang-format` using the repo's `.clang-format`. Match the existing
style (2-space indent, attached braces, the `NVGJS_*` macro patterns) rather than reformatting.

## Track newly discovered bugs

When you discover a bug that isn't the one you're currently fixing, append it to the end of the
`BUGS` file (create it if it doesn't exist) instead of fixing it inline or silently ignoring it.
Don't fix unrelated bugs without asking first.

Format each entry like `../../../shish/BUGS`:

```
- <canonical-name>: <description, wrapped prose explaining what's wrong,
  how it was found, and any relevant root-cause detail>

    <JS repro that triggers it>

```

Blank line between the description and the repro, and a blank line after the repro before the
next entry.
