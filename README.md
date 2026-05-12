# qjs-nanovg Reference

QuickJS bindings for NanoVG — a small antialiased 2D vector graphics library
modelled after the HTML5 Canvas API, rendered with OpenGL.

Import: `import { NVGContext, RGB, RGBA, ... } from 'nanovg';`

## Typical render loop (with GLFW)

```js
import { Window, poll, CONTEXT_VERSION_MAJOR, CONTEXT_VERSION_MINOR,
         OPENGL_PROFILE, OPENGL_CORE_PROFILE, OPENGL_FORWARD_COMPAT } from 'glfw';
import { NVGContext, RGBA } from 'nanovg';

Window.hint(CONTEXT_VERSION_MAJOR, 3);
Window.hint(CONTEXT_VERSION_MINOR, 2);
Window.hint(OPENGL_PROFILE, OPENGL_CORE_PROFILE);
Window.hint(OPENGL_FORWARD_COMPAT, true);

const win = new Window(800, 600, 'NanoVG');
win.makeContextCurrent();

const vg = new NVGContext();

while (!win.shouldClose) {
  const { width, height } = win.size;
  const { width: fw, height: fh } = win.framebufferSize;

  vg.beginFrame(width, height, fw / width);

  // --- draw here ---
  vg.beginPath();
  vg.rect(100, 100, 200, 150);
  vg.fillColor(RGBA(255, 128, 0, 255));
  vg.fill();
  // -----------------

  vg.endFrame();
  win.swapBuffers();
  poll();
}
```

## `NVGContext` class

### Constructor

```js
const vg = new NVGContext([flags]);
// flags: NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG
```

### Frame

```js
vg.beginFrame(windowWidth, windowHeight, devicePixelRatio)
vg.cancelFrame()
vg.endFrame()
```

### State save/restore

```js
vg.save()      // push current state
vg.restore()   // pop state
vg.reset()     // reset to defaults
```

### Paths

```js
vg.beginPath()
vg.moveTo(x, y)
vg.lineTo(x, y)
vg.bezierTo(c1x, c1y, c2x, c2y, x, y)
vg.quadTo(cx, cy, x, y)
vg.arcTo(x1, y1, x2, y2, radius)
vg.arc(cx, cy, r, a0, a1, dir)   // dir: NVG_CW or NVG_CCW
vg.rect(x, y, w, h)
vg.roundedRect(x, y, w, h, r)
vg.roundedRectVarying(x, y, w, h, rtl, rtr, rbr, rbl)
vg.ellipse(cx, cy, rx, ry)
vg.circle(cx, cy, r)
vg.closePath()
vg.pathWinding(dir)   // NVG_SOLID or NVG_HOLE
```

### Fill & Stroke

```js
vg.fill()
vg.stroke()
vg.fillColor(color)          // color from RGB() / RGBA()
vg.fillPaint(paint)          // paint from gradient / imagePattern
vg.strokeColor(color)
vg.strokePaint(paint)
vg.strokeWidth(width)
vg.miterLimit(limit)
vg.lineCap(cap)    // NVG_BUTT, NVG_ROUND, NVG_SQUARE
vg.lineJoin(join)  // NVG_MITER, NVG_ROUND, NVG_BEVEL
vg.globalAlpha(alpha)
```

### Transforms

```js
vg.resetTransform()
vg.transform(a, b, c, d, e, f)   // raw 2D matrix
vg.translate(x, y)
vg.rotate(angle)                  // radians
vg.skewX(angle)
vg.skewY(angle)
vg.scale(x, y)
vg.currentTransform()             // → [a,b,c,d,e,f]
```

### Clipping/Scissors

```js
vg.scissor(x, y, w, h)
vg.intersectScissor(x, y, w, h)
vg.resetScissor()
```

### Gradients & Paints (return paint objects)

```js
vg.linearGradient(sx, sy, ex, ey, icol, ocol)
vg.boxGradient(x, y, w, h, r, f, icol, ocol)
vg.radialGradient(cx, cy, inr, outr, icol, ocol)
vg.imagePattern(ox, oy, ex, ey, angle, image, alpha)
```

### Images

```js
const img = vg.createImage(filename, flags)
const img = vg.createImageMem(flags, data)         // data: ArrayBuffer
const img = vg.createImageRGBA(w, h, flags, data)
vg.updateImage(image, data)
vg.imageSize(image)   // → { w, h }
vg.deleteImage(image)
```

Image flags: `NVG_IMAGE_GENERATE_MIPMAPS`, `NVG_IMAGE_REPEATX`, `NVG_IMAGE_REPEATY`,
`NVG_IMAGE_FLIPY`, `NVG_IMAGE_PREMULTIPLIED`, `NVG_IMAGE_NEAREST`.

### Fonts & Text

```js
const font = vg.createFont(name, filename)
const font = vg.createFontMem(name, data)
vg.addFallbackFont(baseFont, fallbackFont)
vg.fontSize(size)
vg.fontBlur(blur)
vg.textLetterSpacing(spacing)
vg.textLineHeight(lineHeight)
vg.textAlign(align)   // NVG_ALIGN_LEFT|CENTER|RIGHT  + NVG_ALIGN_TOP|MIDDLE|BOTTOM|BASELINE
vg.fontFaceId(font)
vg.fontFace(name)
vg.text(x, y, string)
vg.textBox(x, y, breakRowWidth, string)
vg.textBounds(x, y, string)   // → { minx, miny, maxx, maxy, advance }
vg.textBoxBounds(x, y, breakRowWidth, string)   // → bounds
vg.textMetrics()              // → { ascender, descender, lineh }
```

Text align constants: `NVG_ALIGN_LEFT`, `NVG_ALIGN_CENTER`, `NVG_ALIGN_RIGHT`,
`NVG_ALIGN_TOP`, `NVG_ALIGN_MIDDLE`, `NVG_ALIGN_BOTTOM`, `NVG_ALIGN_BASELINE`.

## Color helpers (free functions)

```js
RGB(r, g, b)                    // → NVGcolor, components 0–255
RGBA(r, g, b, a)                // → NVGcolor, components 0–255
RGBf(r, g, b)                   // → NVGcolor, components 0.0–1.0
RGBAf(r, g, b, a)
LerpRGBA(c0, c1, u)             // → interpolated color
TransRGBA(c, a)                 // → color with new alpha (0–255)
TransRGBAf(c, a)                // → color with new alpha (0.0–1.0)
HSL(h, s, l)
HSLA(h, s, l, a)
```

## Context flags

`NVG_ANTIALIAS`, `NVG_STENCIL_STROKES`, `NVG_DEBUG`

## Winding

`NVG_CW`, `NVG_CCW`, `NVG_SOLID`, `NVG_HOLE`
