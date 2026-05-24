# qjs-nanovg API Documentation

JavaScript bindings for [NanoVG](https://github.com/memononen/nanovg) exposed as the QuickJS
native module `nanovg`. This document is generated from the actual exports in
`nvgjs-module.c` — it is the authoritative reference for names and signatures (the top-level
`README.md` documents an older, lowercase API that does **not** match the code).

```js
import {
  CreateGL3, DeleteGL3, RGB, RGBA, ANTIALIAS, STENCIL_STROKES, Transform,
} from 'nanovg';
```

Run scripts with `qjsm` (the module-enabled QuickJS), which loads the installed `nanovg.so`.
A current OpenGL context must already exist (typically created via the separate `glfw` module)
before calling `CreateGL3`.

> **Conventions**
> - All method and function names are **PascalCase**.
> - Angles are in **radians** (use `DegToRad`/`RadToDeg` to convert).
> - Image and font handles are plain integer ids.
> - A *color argument* accepts a `Color` object or a plain array `[r, g, b]` / `[r, g, b, a]`
>   of **0..1 floats** (this is the raw `NVGcolor` layout, regardless of how the color was
>   constructed).

---

## Module exports

### Named class/object exports

| Export | Kind | Notes |
|--------|------|-------|
| `Context` | constructor object | The `NVGcontext` wrapper. **Do not use `new Context()`** — obtain an instance from `CreateGL3()`. All drawing methods live on its prototype. |
| `Color` | constructor object | A `Float32Array(4)` (r, g, b, a as 0..1 floats) with `r`/`g`/`b`/`a` accessors. Produced by the color helper functions. |
| `Transform` | object | Holds the static transform helpers (`Transform.Identity`, `Transform.Translate`, …). Returned transform values are `Float32Array(6)`. |
| `Paint` | class | Opaque `NVGpaint` wrapper returned by gradient / image-pattern methods; passed to `FillPaint`/`StrokePaint`. |

### Free functions

These are exported directly from the module.

#### Context lifecycle (OpenGL 3 backend — this build)

| Function | Returns | Description |
|----------|---------|-------------|
| `CreateGL3(flags)` | `Context` | Initializes GLEW and creates an NVG context. `flags` is an OR of `ANTIALIAS`, `STENCIL_STROKES`, `DEBUG`. |
| `DeleteGL3(ctx)` | `undefined` | Destroys the context and clears its internal pointer. |
| `CreateImageFromHandleGL3(ctx, textureId, w, h, imageFlags)` | image id | Wraps an existing GL texture as an NVG image. |
| `ImageHandleGL3(ctx, image)` | GL texture id | Returns the GL texture handle backing an NVG image. |

*(`CreateGL2`, `DeleteGL2`, `CreateImageFromHandleGL2`, `ImageHandleGL2` exist only when the
module is compiled with `NANOVG_GL2`; this build uses GL3.)*

#### Framebuffers

| Function | Returns | Description |
|----------|---------|-------------|
| `CreateFramebuffer(ctx, w, h, imageFlags)` | framebuffer object | Creates an offscreen `NVGLUframebuffer`. Throws on failure. |
| `BindFramebuffer(fb)` | `undefined` | Binds the framebuffer for rendering; pass `null` to bind the default framebuffer. |
| `DeleteFramebuffer(fb)` | `undefined` | Destroys the framebuffer. |

#### Pixel read-back

| Function | Returns | Description |
|----------|---------|-------------|
| `ReadPixels(w, h)` | `ArrayBuffer` | Reads the `(0,0,w,h)` region of the current GL framebuffer as RGBA bytes (`w*h*4` bytes). |

#### Angle helpers

| Function | Returns | Description |
|----------|---------|-------------|
| `DegToRad(deg)` | number | Degrees → radians. |
| `RadToDeg(rad)` | number | Radians → degrees. |

#### Color constructors

All return a `Color` object.

| Function | Description |
|----------|-------------|
| `RGB(r, g, b)` | Components 0..255 (alpha = 255). |
| `RGBf(r, g, b)` | Components 0..1 (alpha = 1). |
| `RGBA(r, g, b, a)` | Components 0..255. |
| `RGBAf(r, g, b, a)` | Components 0..1. |
| `LerpRGBA(c0, c1, u)` | Linear interpolation between two colors; `u` in 0..1. |
| `TransRGBA(c, a)` | Copy of `c` with integer alpha `a` (0..255). |
| `TransRGBAf(c, a)` | Copy of `c` with float alpha `a` (0..1). |
| `HSL(h, s, l)` | From hue/saturation/lightness (alpha = 1). |
| `HSLA(h, s, l, a)` | HSL with integer alpha `a` (0..255). |

#### Standalone transform

| Function | Returns | Description |
|----------|---------|-------------|
| `TransformPoint(out, transform, ...points)` | number | Transforms points by the 6-element `transform` matrix. Writes results into the `out` `Float32Array`; with extra `points` args it transforms those, otherwise it transforms the points already in `out` in place. Returns the number of points written. |

---

## Constants

Exported as module properties (integers unless noted).

- **Create flags:** `ANTIALIAS`, `STENCIL_STROKES`, `DEBUG`
- **Math:** `PI` (number)
- **Winding / solidity:** `CCW`, `CW`, `SOLID`, `HOLE`
- **Line cap:** `BUTT`, `ROUND`, `SQUARE`
- **Line join:** `MITER`, `ROUND`, `BEVEL`
- **Text align:** `ALIGN_LEFT`, `ALIGN_CENTER`, `ALIGN_RIGHT`, `ALIGN_TOP`, `ALIGN_MIDDLE`,
  `ALIGN_BOTTOM`, `ALIGN_BASELINE`
- **Image flags:** `IMAGE_GENERATE_MIPMAPS`, `IMAGE_REPEATX`, `IMAGE_REPEATY`, `IMAGE_FLIPY`,
  `IMAGE_PREMULTIPLIED`, `IMAGE_NEAREST`, `IMAGE_NODELETE`
- **Texture type:** `TEXTURE_ALPHA`, `TEXTURE_RGBA`
- **Blend factors:** `ZERO`, `ONE`, `SRC_COLOR`, `ONE_MINUS_SRC_COLOR`, `DST_COLOR`,
  `ONE_MINUS_DST_COLOR`, `SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA`, `DST_ALPHA`,
  `ONE_MINUS_DST_ALPHA`, `SRC_ALPHA_SATURATE`
- **Composite operations:** `SOURCE_OVER`, `SOURCE_IN`, `SOURCE_OUT`, `ATOP`,
  `DESTINATION_OVER`, `DESTINATION_IN`, `DESTINATION_OUT`, `DESTINATION_ATOP`, `LIGHTER`,
  `COPY`, `XOR`

---

## `Context` methods

All methods are invoked on a context returned by `CreateGL3()`.

### Frame

| Method | Description |
|--------|-------------|
| `BeginFrame(windowWidth, windowHeight, devicePixelRatio)` | Begins a drawing frame. |
| `EndFrame()` | Flushes and renders the frame. |
| `CancelFrame()` | Discards the current frame without rendering. |

### State

| Method | Description |
|--------|-------------|
| `Save()` | Pushes the current render state. |
| `Restore()` | Pops the render state. |
| `Reset()` | Resets the render state to defaults. |
| `ShapeAntiAlias(enabled)` | Enables/disables shape antialiasing (boolean). |

### Path building

| Method | Description |
|--------|-------------|
| `BeginPath()` | Clears the current path. |
| `MoveTo(x, y)` | Starts a new sub-path. |
| `LineTo(x, y)` | Adds a line segment. |
| `BezierTo(c1x, c1y, c2x, c2y, x, y)` | Cubic bézier segment. |
| `QuadTo(cx, cy, x, y)` | Quadratic bézier segment. |
| `ArcTo(x1, y1, x2, y2, radius)` | Arc between two tangent lines. |
| `Arc(cx, cy, r, a0, a1, dir)` | Arc; `dir` is `CW` or `CCW`. |
| `Rect(x, y, w, h)` | Rectangle. |
| `RoundedRect(x, y, w, h, r)` | Rounded rectangle (uniform radius). |
| `RoundedRectVarying(x, y, w, h, rtl, rtr, rbr, rbl)` | Rounded rectangle with per-corner radii. |
| `Ellipse(cx, cy, rx, ry)` | Ellipse. |
| `Circle(cx, cy, r)` | Circle. |
| `ClosePath()` | Closes the current sub-path. |
| `PathWinding(dir)` | Sets winding of the current sub-path (`SOLID`/`HOLE` or `CW`/`CCW`). |

### Fill & stroke style

| Method | Description |
|--------|-------------|
| `Fill()` | Fills the current path. |
| `Stroke()` | Strokes the current path. |
| `FillColor(color)` | Sets fill to a solid color. |
| `FillPaint(paint)` | Sets fill to a `Paint` (gradient / image pattern). |
| `StrokeColor(color)` | Sets stroke to a solid color. |
| `StrokePaint(paint)` | Sets stroke to a `Paint`. |
| `StrokeWidth(width)` | Sets stroke width. |
| `MiterLimit(limit)` | Sets the miter limit. |
| `LineCap(cap)` | `BUTT`, `ROUND`, or `SQUARE`. |
| `LineJoin(join)` | `MITER`, `ROUND`, or `BEVEL`. ⚠️ *Known bug: currently forwards to `nvgLineCap`.* |
| `GlobalAlpha(alpha)` | Global alpha multiplier (0..1). |

### Gradients & paints

Each returns a `Paint` object for use with `FillPaint`/`StrokePaint`.

| Method | Description |
|--------|-------------|
| `LinearGradient(sx, sy, ex, ey, icol, ocol)` | Linear gradient between two points. |
| `BoxGradient(x, y, w, h, r, f, icol, ocol)` | Box gradient (`r` = corner radius, `f` = feather). |
| `RadialGradient(cx, cy, inr, outr, icol, ocol)` | Radial gradient. |
| `ImagePattern(ox, oy, ex, ey, angle, image, alpha)` | Repeating image pattern from an image id. |

### Scissor (clipping)

| Method | Description |
|--------|-------------|
| `Scissor(x, y, w, h)` | Sets the clip rectangle. |
| `IntersectScissor(x, y, w, h)` | Intersects with the current clip rectangle. |
| `ResetScissor()` | Clears scissoring. |

### Transforms

Angles in radians. The matrix order is `[a, b, c, d, e, f]`.

| Method | Description |
|--------|-------------|
| `ResetTransform()` | Resets to the identity transform. |
| `Transform(a, b, c, d, e, f)` | Premultiplies a raw matrix (also accepts a single 6-element array). |
| `Translate(x, y)` | Translates. |
| `Rotate(angle)` | Rotates. |
| `SkewX(angle)` | Skews along X. |
| `SkewY(angle)` | Skews along Y. |
| `Scale(x, y)` | Scales. |
| `CurrentTransform([out])` | Returns the current transform as a new `Transform`; if `out` is given, copies into it and returns `undefined`. |

### Images

| Method | Returns | Description |
|--------|---------|-------------|
| `CreateImage(filename, flags)` | image id | Loads an image from a file. |
| `CreateImageMem(flags, data)` | image id | Decodes an image from an `ArrayBuffer`. |
| `CreateImageRGBA(w, h, flags, data)` | image id | Creates an image from raw RGBA bytes in an `ArrayBuffer`. |
| `UpdateImage(image, data)` | `undefined` | Replaces image pixel data from an `ArrayBuffer`. |
| `ImageSize(image)` | `[w, h]` | Returns image dimensions as a 2-element array. |
| `DeleteImage(image)` | `undefined` | Frees an image. |

### Fonts & text

| Method | Returns | Description |
|--------|---------|-------------|
| `CreateFont(name, filename)` | font id | Registers a font from a file. |
| `CreateFontAtIndex(name, filename, index)` | font id | Registers a specific face from a font collection. |
| `FindFont(name)` | font id / -1 | Looks up a previously created font by name. |
| `FontSize(size)` | `undefined` | Sets font size (px). |
| `FontBlur(blur)` | `undefined` | Sets font blur. |
| `FontFace(name)` | `undefined` | Selects the active font by name. |
| `TextLetterSpacing(spacing)` | `undefined` | Sets letter spacing. |
| `TextLineHeight(lineHeight)` | `undefined` | Sets line height (multiplier). |
| `TextAlign(align)` | `undefined` | OR of `ALIGN_*` horizontal + vertical constants. |
| `Text(x, y, string [, charEnd])` | advance (number) | Draws text. Optional `charEnd` limits drawing to the first N characters (Unicode-aware). |
| `TextBox(x, y, breakRowWidth, string [, charEnd])` | `undefined` | Draws word-wrapped text. |
| `TextBounds(x, y, string, charEnd, out)` | advance (number) | Measures text; writes `{xmin, ymin, xmax, ymax}` into the `out` object. Pass `null`/`undefined` for `charEnd` to measure the whole string. |
| `TextBoxBounds(x, y, breakRowWidth, string, charEnd, out)` | `undefined` | Measures wrapped text; writes `{xmin, ymin, xmax, ymax}` into `out`. |
| `TextBounds2(x, y, string)` | `{ width, height }` | Convenience measurement returning a plain object. |

---

## `Transform` helpers

`Transform` values are `Float32Array(6)` matrices. They have indexed accessors
`a,b,c,d,e,f` and aliases `xx,yx,xy,yy,x0,y0`.

### Static functions (on the `Transform` export)

These build/modify matrices. When called without a target matrix they return a new `Transform`;
when the first argument is a matrix they operate on it in place. They are chainable (return the
matrix), e.g. `Transform.Scale(3, 3).Rotate(angle).Translate(1, 1)`.

| Function | Description |
|----------|-------------|
| `Transform.Identity([mat])` | Identity matrix. |
| `Transform.Translate([mat,] x, y)` | Translation. |
| `Transform.Scale([mat,] x, y)` | Scaling. |
| `Transform.Rotate([mat,] angle)` | Rotation (radians). |
| `Transform.SkewX([mat,] angle)` | X skew. |
| `Transform.SkewY([mat,] angle)` | Y skew. |
| `Transform.Multiply(mat, ...matrices)` | Post-multiplies `mat` by the given matrices. |
| `Transform.Premultiply(mat, ...matrices)` | Pre-multiplies `mat` by the given matrices. |
| `Transform.Inverse(mat)` | Inverts `mat`. Throws if non-invertible. |

### Instance methods (on a transform value)

A transform value also carries chainable instance methods `Translate`, `Scale`, `Rotate`,
`SkewX`, `SkewY`, `Multiply`, `Premultiply`, `Inverse`, and `TransformPoint(x, y)` (which
returns the transformed point as `[x, y]`).

---

## Minimal example

```js
import * as glfw from 'glfw';
import { CreateGL3, STENCIL_STROKES, ANTIALIAS, RGB, RGBA } from 'nanovg';

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

See `test-nanovg.js` in the repository root for a fuller, animated example.
