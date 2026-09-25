import createModule from './nanovg-web.js';

const m = await createModule();

const InternalError =
  globalThis.InternalError ??
  class InternalError extends Error {
    name = 'InternalError';
  };

export const PI = Math.fround(Math.PI);

export const STENCIL_STROKES = 1 << 1;
export const ANTIALIAS = 1 << 0;
export const DEBUG = 1 << 2;
export const IMAGE_NODELETE = 1 << 16;
export const CCW = 1;
export const CW = 2;
export const SOLID = 1;
export const HOLE = 2;
export const BUTT = 0;
export const ROUND = 1;
export const SQUARE = 2;
export const BEVEL = 3;
export const MITER = 4;
export const ALIGN_LEFT = 1 << 0;
export const ALIGN_CENTER = 1 << 1;
export const ALIGN_RIGHT = 1 << 2;
export const ALIGN_TOP = 1 << 3;
export const ALIGN_MIDDLE = 1 << 4;
export const ALIGN_BOTTOM = 1 << 5;
export const ALIGN_BASELINE = 1 << 6;
export const ZERO = 1 << 0;
export const ONE = 1 << 1;
export const SRC_COLOR = 1 << 2;
export const ONE_MINUS_SRC_COLOR = 1 << 3;
export const DST_COLOR = 1 << 4;
export const ONE_MINUS_DST_COLOR = 1 << 5;
export const SRC_ALPHA = 1 << 6;
export const ONE_MINUS_SRC_ALPHA = 1 << 7;
export const DST_ALPHA = 1 << 8;
export const ONE_MINUS_DST_ALPHA = 1 << 9;
export const SRC_ALPHA_SATURATE = 1 << 10;
export const SOURCE_OVER = 0;
export const SOURCE_IN = 1;
export const SOURCE_OUT = 2;
export const ATOP = 3;
export const DESTINATION_OVER = 4;
export const DESTINATION_IN = 5;
export const DESTINATION_OUT = 6;
export const DESTINATION_ATOP = 7;
export const LIGHTER = 8;
export const COPY = 9;
export const XOR = 10;
export const IMAGE_GENERATE_MIPMAPS = 1 << 0;
export const IMAGE_REPEATX = 1 << 1;
export const IMAGE_REPEATY = 1 << 2;
export const IMAGE_FLIPY = 1 << 3;
export const IMAGE_PREMULTIPLIED = 1 << 4;
export const IMAGE_NEAREST = 1 << 5;
export const TEXTURE_ALPHA = 1;
export const TEXTURE_RGBA = 2;

const isObject = v => v !== null && (typeof v == 'object' || typeof v == 'function');
const isF32 = v => v instanceof Float32Array;

const def = (target, name, length, impl) =>
  Object.defineProperty(target, name, {
    value: Object.defineProperties(impl, { name: { value: name }, length: { value: length } }),
    writable: true,
    configurable: true,
  });

const tag = (proto, name) => Object.defineProperty(proto, Symbol.toStringTag, { value: name, configurable: true });

const getter = (proto, name, get, set) => Object.defineProperty(proto, name, { get, set, configurable: true });

const need = (args, n) => {
  if(args.length < n) throw new InternalError(`need ${n} arguments`);
};

// Module-level function with the native declared length and argc requirement.
const func = (name, length, needed, impl) => {
  const f = (...args) => {
    need(args, needed);
    return impl(args);
  };
  return Object.defineProperties(f, { name: { value: name }, length: { value: length } });
};

const withString = (s, fn) => {
  const size = m.lengthBytesUTF8(s) + 1;
  const ptr = m._malloc(size);
  m.stringToUTF8(s, ptr, size);
  try {
    return fn(ptr);
  } finally {
    m._free(ptr);
  }
};

const bytesOf = v => {
  if(v instanceof ArrayBuffer) return new Uint8Array(v);
  if(ArrayBuffer.isView(v)) return new Uint8Array(v.buffer, v.byteOffset, v.byteLength);
  throw new TypeError('expecting an ArrayBuffer');
};

// The C side reads w*h*4 bytes without checking, so pad short buffers rather than read past the allocation.
const withBytes = (v, minLength, fn) => {
  const bytes = bytesOf(v);
  const size = Math.max(bytes.length, minLength);
  const ptr = m._malloc(size);
  m.HEAPU8.set(bytes, ptr);
  m.HEAPU8.fill(0, ptr + bytes.length, ptr + size);
  try {
    return fn(ptr);
  } finally {
    m._free(ptr);
  }
};

const f32 = (ptr, n) => new Float32Array(m.HEAPU8.buffer, ptr, n);

// Node-style file access for CreateFont/CreateImage: they read from Emscripten's in-memory FS.
export const writeFile = (path, data) => {
  const dir = path.slice(0, path.lastIndexOf('/'));
  if(dir) m.FS.mkdirTree(dir);
  m.FS.writeFile(path, bytesOf(data));
};

const invalid = name => new TypeError(`${name} object expected`);

/* Colors */

export class Color extends Float32Array {
  static get [Symbol.species]() {
    return Float32Array;
  }
}
tag(Color.prototype, 'nvgColor');
['r', 'g', 'b', 'a'].forEach((k, i) =>
  getter(
    Color.prototype,
    k,
    function() {
      return this[i];
    },
    function(v) {
      this[i] = v;
    },
  ),
);

const color = (r, g, b, a) => new Color([r, g, b, a]);

const toColor = v => {
  if(!isObject(v)) throw new TypeError('expecting a Float32Array, Array or Iterable');
  if(v.length >= 4) return [+v[0], +v[1], +v[2], +v[3]];
  if(v.length >= 3) return [+v[0], +v[1], +v[2], 1];
  throw new RangeError('input Array must have at least 3 elements');
};

const clampf = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);

const hue = (h, m1, m2) => {
  if(h < 0) h += 1;
  if(h > 1) h -= 1;
  if(h < 1 / 6) return m1 + (m2 - m1) * h * 6;
  if(h < 3 / 6) return m2;
  if(h < 4 / 6) return m1 + (m2 - m1) * (2 / 3 - h) * 6;
  return m1;
};

const hsla = (h, s, l, a) => {
  h = h % 1;
  if(h < 0) h += 1;
  s = clampf(s, 0, 1);
  l = clampf(l, 0, 1);
  const m2 = l <= 0.5 ? l * (1 + s) : l + s - l * s;
  const m1 = 2 * l - m2;
  return color(clampf(hue(h + 1 / 3, m1, m2), 0, 1), clampf(hue(h, m1, m2), 0, 1), clampf(hue(h - 1 / 3, m1, m2), 0, 1), (a & 255) / 255);
};

export const DegToRad = func('DegToRad', 1, 1, ([deg]) => Math.fround(Math.fround(Math.fround(+deg) / 180) * PI));
export const RadToDeg = func('RadToDeg', 1, 1, ([rad]) => Math.fround(Math.fround(Math.fround(+rad) / PI) * 180));

export const RGB = func('RGB', 3, 3, ([r, g, b]) => color(((r | 0) & 255) / 255, ((g | 0) & 255) / 255, ((b | 0) & 255) / 255, 1));
export const RGBf = func('RGBf', 3, 3, ([r, g, b]) => color(+r, +g, +b, 1));
export const RGBA = func('RGBA', 4, 4, ([r, g, b, a]) => color(((r | 0) & 255) / 255, ((g | 0) & 255) / 255, ((b | 0) & 255) / 255, ((a | 0) & 255) / 255));
export const RGBAf = func('RGBAf', 4, 4, ([r, g, b, a]) => color(+r, +g, +b, +a));
export const LerpRGBA = func('LerpRGBA', 3, 3, ([c0, c1, u]) => {
  const a = toColor(c0);
  const b = toColor(c1);
  u = Math.fround(clampf(+u, 0, 1));
  const inv = Math.fround(1 - u);
  return new Color(a.map((v, i) => Math.fround(Math.fround(Math.fround(v) * inv) + Math.fround(Math.fround(b[i]) * u))));
});
export const TransRGBA = func('TransRGBA', 2, 2, ([c, a]) => {
  const v = toColor(c);
  return color(v[0], v[1], v[2], ((a | 0) & 255) / 255);
});
export const TransRGBAf = func('TransRGBAf', 2, 2, ([c, a]) => {
  const v = toColor(c);
  return color(v[0], v[1], v[2], +a);
});
export const HSL = func('HSL', 3, 3, ([h, s, l]) => hsla(+h, +s, +l, 255));
export const HSLA = func('HSLA', 4, 4, ([h, s, l, a]) => hsla(+h, +s, +l, a | 0));

/* Transforms */

const identity = t => t.set([1, 0, 0, 1, 0, 0]);

const mul = (t, s) => {
  const t0 = t[0] * s[0] + t[1] * s[2];
  const t2 = t[2] * s[0] + t[3] * s[2];
  const t4 = t[4] * s[0] + t[5] * s[2] + s[4];
  t[1] = t[0] * s[1] + t[1] * s[3];
  t[3] = t[2] * s[1] + t[3] * s[3];
  t[5] = t[4] * s[1] + t[5] * s[3] + s[5];
  t[0] = t0;
  t[2] = t2;
  t[4] = t4;
};

const premul = (t, s) => {
  const s2 = Float32Array.from(s);
  mul(s2, t);
  t.set(s2);
};

const invert = (inv, t) => {
  const det = t[0] * t[3] - t[2] * t[1];
  if(det > -1e-6 && det < 1e-6) {
    identity(inv);
    return false;
  }
  const d = 1 / det;
  inv.set([t[3] * d, -t[1] * d, -t[2] * d, t[0] * d, (t[2] * t[5] - t[3] * t[4]) * d, (t[1] * t[4] - t[0] * t[5]) * d]);
  return true;
};

const setters = {
  Translate: (t, [x, y]) => t.set([1, 0, 0, 1, x, y]),
  Scale: (t, [x, y]) => t.set([x, 0, 0, y, 0, 0]),
  Rotate: (t, [a]) => t.set([Math.cos(a), Math.sin(a), -Math.sin(a), Math.cos(a), 0, 0]),
  SkewX: (t, [a]) => t.set([1, 0, Math.tan(a), 1, 0, 0]),
  SkewY: (t, [a]) => t.set([1, Math.tan(a), 0, 1, 0, 0]),
};

const readVec = (v, n) => (isObject(v) && v.length >= n ? Array.from({ length: n }, (_, i) => Math.fround(+v[i])) : null);

// nvgjs_arguments(): one vector argument, or n scalars. Returns the vector and how many arguments it used.
const takeVec = (args, n) => {
  const vec = args.length >= 1 && readVec(args[0], n);
  if(vec) return { vec, used: 1 };
  if(args.length >= n) return { vec: args.slice(0, n).map(x => Math.fround(+x)), used: n };
  return null;
};

// nvgjs_inputoutputarray(): a Float32Array is used in place, anything else array-like is copied into tmp.
const inOut = (v, tmp, n = 6) => {
  if(isF32(v)) {
    if(v.length < n) throw new RangeError(`TypedArray vector must have at least ${n} elements (has ${v.length})`);
    return v;
  }
  const vec = readVec(v, n);
  if(!vec) throw new TypeError('expecting a Float32Array, Array or Iterable');
  tmp.set(vec);
  return tmp;
};

const tryInOut = (v, tmp) => {
  try {
    return inOut(v, tmp);
  } catch {
    return null;
  }
};

const KEYS = ['a', 'b', 'c', 'd', 'e', 'f'];

const copyBack = (v, t) => {
  if(!isObject(v)) return;
  if(Array.isArray(v)) t.forEach((x, i) => (v[i] = x));
  else KEYS.forEach((k, i) => (v[k] = t[i]));
};

const selfMat = t => {
  if(!isF32(t) || t.length < 6) throw new TypeError('expecting a Float32Array');
  return t;
};

export class Transform extends Float32Array {
  static get [Symbol.species]() {
    return Float32Array;
  }
}
tag(Transform.prototype, 'nvgTransform');

KEYS.forEach((k, i) =>
  getter(
    Transform.prototype,
    k,
    function() {
      return this[i];
    },
    function(v) {
      this[i] = v;
    },
  ),
);
['xx', 'yx', 'xy', 'yy', 'x0', 'y0'].forEach((k, i) =>
  getter(
    Transform.prototype,
    k,
    function() {
      return this[i];
    },
    function(v) {
      this[i] = v;
    },
  ),
);

// Static form builds a new matrix (or fills one passed first); instance form premultiplies onto this one.
const staticResult = (mat, tmp, i, args) => {
  if(mat !== tmp) return undefined;
  if(!i) return new Transform(tmp);
  copyBack(args[0], tmp);
  return undefined;
};

def(Transform, 'Identity', 0, (...args) => {
  const tmp = new Float32Array(6);
  let mat = tmp;
  let i = 0;
  if(args.length > 0) {
    mat = inOut(args[0], tmp);
    i++;
  }
  identity(mat);
  return staticResult(mat, tmp, i, args);
});

const staticOps = {
  Translate: [2, 'need x, y arguments', 2],
  Scale: [2, 'need x, y or vector arguments', 2],
  Rotate: [1, 'need angle argument', 1],
  SkewX: [1, 'need angle argument', 1],
  SkewY: [1, 'need angle argument', 1],
};

for(const [name, [length, message, n]] of Object.entries(staticOps)) {
  def(Transform, name, length, (...args) => {
    const tmp = new Float32Array(6);
    let mat = tmp;
    let i = 0;
    if(args.length > 1 && isObject(args[0]) && (mat = tryInOut(args[0], tmp))) i++;
    else mat = tmp;

    let vec;
    if(n == 2) {
      const taken = takeVec(args.slice(i), 2);
      if(!taken) throw new InternalError(message);
      vec = taken.vec;
    } else {
      if(args.length < 1 + i) throw new InternalError(message);
      vec = [+args[i]];
    }
    setters[name](mat, vec);
    return staticResult(mat, tmp, i, args);
  });

  def(Transform.prototype, name, length, function(...args) {
    const mat = selfMat(this);
    const t = new Float32Array(6);
    let vec;
    if(name == 'Translate') vec = [+args[0], +args[1]];
    else if(name == 'Scale') vec = [args.length > 0 ? +args[0] : 1, args.length > 1 ? +args[1] : args.length > 0 ? +args[0] : 1];
    else vec = [+args[0]];
    setters[name](t, vec);
    premul(mat, t);
    return this;
  });
}

for(const [name, op] of [
  ['Multiply', mul],
  ['Premultiply', premul],
]) {
  def(Transform, name, 1, (...args) => {
    const tmp = new Float32Array(6);
    let mat = tmp;
    let i = 0;
    if(args.length > 1 && (mat = tryInOut(args[0], tmp))) i++;
    else mat = tmp;

    if(mat === tmp && i == 0) identity(tmp);
    if(args.length < 1 + i) throw new InternalError(`need ${1 + i} arguments`);

    for(let taken; i < args.length && (taken = takeVec(args.slice(i), 6)); i += taken.used) op(mat, taken.vec);

    if(mat === tmp) return i ? (copyBack(args[0], tmp), undefined) : new Transform(tmp);
    return args[0];
  });

  def(Transform.prototype, name, 1, function(...args) {
    const mat = selfMat(this);
    const taken = takeVec(args, 6);
    op(mat, taken ? taken.vec : [1, 0, 0, 1, 0, 0]);
    return this;
  });
}

def(Transform, 'Inverse', 1, (...args) => {
  const tmp = new Float32Array(6);
  let mat = tmp;
  let i = 0;
  if(args.length > 0 && (mat = tryInOut(args[0], tmp))) i++;
  else mat = tmp;

  if(args.length < 1 + i) throw new InternalError(`need ${1 + i} arguments`);
  const src = readVec(args[i], 6);
  if(!src) throw new TypeError('expecting a Float32Array, Array or Iterable');
  if(!invert(mat, src)) throw new InternalError('nvgTransformInverse failed');

  if(mat === tmp) return i ? (copyBack(args[0], tmp), undefined) : new Transform(tmp);
  return args[0];
});

def(Transform.prototype, 'Inverse', 0, function() {
  const mat = selfMat(this);
  invert(mat, Float32Array.from(mat));
  return this;
});

def(Transform.prototype, 'TransformPoint', 1, function(...args) {
  const mat = selfMat(this);
  const [x, y] = takeVec(args, 2)?.vec ?? [0, 0];
  return [Math.fround(x * mat[0] + y * mat[2] + mat[4]), Math.fround(x * mat[1] + y * mat[3] + mat[5])];
});

export const TransformPoint = func('TransformPoint', 2, 2, ([dst, trf, ...src]) => {
  if(!isF32(dst)) throw new TypeError('expecting a Float32Array');
  const t = readVec(trf, 6);
  if(!t) throw new TypeError('expecting a Float32Array, Array or Iterable');

  let i = 0;
  for(let n = dst.length; n >= 2; n -= 2, i++) {
    let p = [dst[2 * i], dst[2 * i + 1]];
    if(src.length) {
      const taken = takeVec(src, 2);
      if(!taken) break;
      p = taken.vec;
      src.splice(0, taken.used);
    }
    dst[2 * i] = p[0] * t[0] + p[1] * t[2] + t[4];
    dst[2 * i + 1] = p[0] * t[1] + p[1] * t[3] + t[5];
  }
  return i;
});

/* Paints */

const paints = new WeakMap();

export class Paint {
  constructor(bytes) {
    paints.set(this, bytes ?? new Uint8Array(m._nvgw_PaintSize()));
  }
}
tag(Paint.prototype, 'nvgPaint');

const paintBytes = v => {
  const bytes = paints.get(v);
  if(!bytes) throw invalid('nvgPaint');
  return bytes;
};

const readPaint = () => {
  const ptr = m._nvgw_PaintPtr();
  return new Paint(m.HEAPU8.slice(ptr, ptr + m._nvgw_PaintSize()));
};

/* Contexts */

const contexts = new WeakMap();

export class Context {}
tag(Context.prototype, 'NVGcontext');

const ptrOf = v => {
  const ptr = contexts.get(v);
  if(!ptr) throw invalid('NVGcontext');
  return ptr;
};

// [declared length, argument kinds, arguments required (default: length), result]
// kinds: f float, i int32, b bool, c color (4 floats), p paint (staged in the C paint buffer), s string
const SPEC = {
  CreateFont: [2, 'ss'],
  CreateFontAtIndex: [3, 'ssi'],
  FindFont: [1, 's'],
  BeginFrame: [3, 'fff'],
  CancelFrame: [0, ''],
  EndFrame: [0, ''],
  Save: [0, ''],
  Restore: [0, ''],
  Reset: [0, ''],
  ShapeAntiAlias: [1, 'b', 0],
  ClosePath: [0, ''],
  Scissor: [4, 'ffff'],
  IntersectScissor: [4, 'ffff'],
  ResetScissor: [0, ''],
  MiterLimit: [1, 'f', 0],
  LineCap: [1, 'i', 0],
  LineJoin: [1, 'i', 0],
  GlobalAlpha: [1, 'f', 0],
  StrokeColor: [1, 'c'],
  StrokeWidth: [1, 'f'],
  StrokePaint: [1, 'p'],
  FillColor: [1, 'c'],
  FillPaint: [1, 'p'],
  LinearGradient: [6, 'ffffcc', 6, 'paint'],
  BoxGradient: [8, 'ffffffcc', 8, 'paint'],
  RadialGradient: [6, 'ffffcc', 6, 'paint'],
  FontSize: [1, 'f'],
  FontBlur: [1, 'f'],
  TextLetterSpacing: [1, 'f'],
  TextLineHeight: [1, 'f'],
  TextAlign: [1, 'i'],
  FontFace: [1, 's'],
  CreateImage: [2, 'si'],
  DeleteImage: [1, 'i'],
  ResetTransform: [0, ''],
  Translate: [2, 'ff'],
  Rotate: [1, 'f'],
  SkewX: [1, 'f'],
  SkewY: [1, 'f'],
  Scale: [2, 'ff'],
  ImagePattern: [7, 'fffffif', 7, 'paint'],
  BeginPath: [0, ''],
  MoveTo: [2, 'ff'],
  LineTo: [2, 'ff'],
  BezierTo: [6, 'ffffff'],
  QuadTo: [4, 'ffff'],
  ArcTo: [5, 'fffff'],
  Arc: [6, 'fffffi'],
  Rect: [4, 'ffff'],
  Circle: [3, 'fff'],
  Ellipse: [4, 'ffff'],
  RoundedRect: [5, 'fffff'],
  RoundedRectVarying: [8, 'ffffffff'],
  PathWinding: [1, 'i'],
  Stroke: [0, ''],
  Fill: [0, ''],
};

for(const [name, [length, kinds, needed = length, result]] of Object.entries(SPEC)) {
  const wasm = m[`_nvgw_${name}`];

  def(Context.prototype, name, length, function(...args) {
    const out = [ptrOf(this)];
    const frees = [];
    need(args, needed);

    try {
      for(let k = 0; k < kinds.length; k++) {
        const a = args[k];
        switch (kinds[k]) {
          case 'f':
            out.push(+a);
            break;
          case 'i':
            out.push(a | 0);
            break;
          case 'b':
            out.push(a ? 1 : 0);
            break;
          case 'c':
            out.push(...toColor(a));
            break;
          case 'p':
            m.HEAPU8.set(paintBytes(a), m._nvgw_PaintPtr());
            break;
          case 's': {
            const s = String(a);
            const size = m.lengthBytesUTF8(s) + 1;
            const ptr = m._malloc(size);
            frees.push(ptr);
            m.stringToUTF8(s, ptr, size);
            out.push(ptr);
            break;
          }
        }
      }

      const ret = wasm(...out);
      return result == 'paint' ? readPaint() : ret;
    } finally {
      frees.forEach(p => m._free(p));
    }
  });
}

// Optional `end` arguments count code points; nvgText wants a byte offset into the UTF-8 string.
const byteOffset = (s, pos) => {
  let bytes = 0;
  let i = 0;
  for(const ch of s) {
    if(i++ >= pos) break;
    const cp = ch.codePointAt(0);
    bytes += cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
  }
  return bytes;
};

const endOf = (s, pos) => (pos === undefined || pos === null ? -1 : byteOffset(s, pos | 0));

const setBounds = (out, b) => {
  if(!isObject(out)) return;
  ['xmin', 'ymin', 'xmax', 'ymax'].forEach((k, i) => (out[k] = b[i]));
};

def(Context.prototype, 'Text', 3, function(...args) {
  const c = ptrOf(this);
  need(args, 3);
  const str = String(args[2]);
  return withString(str, p => m._nvgw_Text(c, +args[0], +args[1], p, endOf(str, args[3])));
});

def(Context.prototype, 'TextBox', 4, function(...args) {
  const c = ptrOf(this);
  need(args, 4);
  const str = String(args[3]);
  withString(str, p => m._nvgw_TextBox(c, +args[0], +args[1], +args[2], p, endOf(str, args[4])));
});

def(Context.prototype, 'TextBounds', 5, function(...args) {
  const c = ptrOf(this);
  need(args, 5);
  const str = String(args[2]);
  const adv = m._malloc(4);
  try {
    return withString(str, p => {
      const b = f32(m._nvgw_TextBounds(c, +args[0], +args[1], p, endOf(str, args[3]), adv), 4);
      setBounds(args[4], Array.from(b));
      return f32(adv, 1)[0];
    });
  } finally {
    m._free(adv);
  }
});

def(Context.prototype, 'TextBoxBounds', 6, function(...args) {
  const c = ptrOf(this);
  need(args, 6);
  const str = String(args[3]);
  withString(str, p => setBounds(args[5], Array.from(f32(m._nvgw_TextBoxBounds(c, +args[0], +args[1], +args[2], p, endOf(str, args[4])), 4))));
});

def(Context.prototype, 'TextBounds2', 3, function(...args) {
  const c = ptrOf(this);
  need(args, 3);
  const adv = m._malloc(4);
  try {
    return withString(String(args[2]), p => {
      const b = f32(m._nvgw_TextBounds(c, +args[0], +args[1], p, -1, adv), 4);
      return { width: f32(adv, 1)[0], height: Math.fround(b[3] - b[1]) };
    });
  } finally {
    m._free(adv);
  }
});

def(Context.prototype, 'CreateImageMem', 2, function(...args) {
  const c = ptrOf(this);
  need(args, 2);
  return withBytes(args[1], 0, p => m._nvgw_CreateImageMem(c, args[0] | 0, p, bytesOf(args[1]).length));
});

def(Context.prototype, 'CreateImageRGBA', 4, function(...args) {
  const c = ptrOf(this);
  need(args, 4);
  const [w, h, flags] = args.map(a => a | 0);
  return withBytes(args[3], w * h * 4, p => m._nvgw_CreateImageRGBA(c, w, h, flags, p));
});

def(Context.prototype, 'UpdateImage', 2, function(...args) {
  const c = ptrOf(this);
  need(args, 2);
  const image = args[0] | 0;
  return void withBytes(args[1], m._nvgw_ImageWidth(c, image) * m._nvgw_ImageHeight(c, image) * 4, p => m._nvgw_UpdateImage(c, image, p));
});

def(Context.prototype, 'ImageSize', 1, function(...args) {
  const c = ptrOf(this);
  need(args, 1);
  const image = args[0] | 0;
  return [m._nvgw_ImageWidth(c, image), m._nvgw_ImageHeight(c, image)];
});

def(Context.prototype, 'Transform', 6, function(...args) {
  const c = ptrOf(this);
  need(args, 1);
  const taken = takeVec(args, 6);
  if(taken) m._nvgw_Transform(c, ...taken.vec);
});

def(Context.prototype, 'CurrentTransform', 1, function(...args) {
  const t = Float32Array.from(f32(m._nvgw_CurrentTransform(ptrOf(this)), 6));
  if(args.length == 0) return new Transform(t);
  copyBack(args[0], t);
});

export const CreateGL3 = func('CreateGL3', 1, 1, ([flags]) => {
  const ptr = m._nvgw_Create(flags | 0);
  if(!ptr) throw new InternalError('nvg.CreateGL3: could not create a WebGL2 context or NanoVG');
  const nvg = Object.create(Context.prototype);
  contexts.set(nvg, ptr);
  return nvg;
});

export const DeleteGL3 = func('DeleteGL3', 1, 0, ([nvg]) => {
  m._nvgw_Delete(ptrOf(nvg));
  contexts.delete(nvg);
});

export const CreateImageFromHandleGL3 = func('CreateImageFromHandleGL3', 5, 0, ([nvg, tex, w, h, flags]) => m._nvgw_CreateImageFromHandle(ptrOf(nvg), tex >>> 0, w | 0, h | 0, flags | 0));

export const ImageHandleGL3 = func('ImageHandleGL3', 2, 0, ([nvg, image]) => m._nvgw_ImageHandle(ptrOf(nvg), image | 0) >>> 0);

/* Framebuffers */

const framebuffers = new WeakMap();

class Framebuffer {}
tag(Framebuffer.prototype, 'NVGLUframebuffer');
['fbo', 'rbo', 'texture', 'image'].forEach((k, i) =>
  getter(Framebuffer.prototype, k, function() {
    const ptr = framebuffers.get(this);
    if(!ptr) throw invalid('NVGLUframebuffer');
    return m._nvgw_FramebufferField(ptr, i);
  }),
);

const fbPtrOf = v => {
  const ptr = framebuffers.get(v);
  if(!ptr) throw invalid('NVGLUframebuffer');
  return ptr;
};

export const CreateFramebuffer = func('CreateFramebuffer', 4, 0, ([nvg, w, h, flags]) => {
  const c = ptrOf(nvg);
  const ptr = m._nvgw_CreateFramebuffer(c, w | 0, h | 0, flags | 0);
  if(!ptr) throw new InternalError(`Failed creating NVGLUframebuffer [${w | 0}x${h | 0}] (${flags | 0})`);
  const fb = Object.create(Framebuffer.prototype);
  framebuffers.set(fb, ptr);
  return fb;
});

export const BindFramebuffer = func('BindFramebuffer', 1, 0, ([fb]) => {
  m._nvgw_BindFramebuffer(fb === null ? 0 : fbPtrOf(fb));
});

export const DeleteFramebuffer = func('DeleteFramebuffer', 1, 0, ([fb]) => {
  m._nvgw_DeleteFramebuffer(fbPtrOf(fb));
  framebuffers.delete(fb);
});

export const ReadPixels = func('ReadPixels', 2, 0, ([w, h]) => {
  w >>>= 0;
  h >>>= 0;
  const size = w * h * 4;
  const ptr = m._malloc(size);
  try {
    m._nvgw_ReadPixels(w, h, ptr);
    return m.HEAPU8.slice(ptr, ptr + size).buffer;
  } finally {
    m._free(ptr);
  }
});
