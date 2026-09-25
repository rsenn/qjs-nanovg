import createModule from './nanovg-web.js';

const m = await createModule();

export const ANTIALIAS = 1;
export const STENCIL_STROKES = 2;
export const ALIGN_LEFT = 1;
export const ALIGN_CENTER = 2;
export const ALIGN_RIGHT = 4;
export const ALIGN_TOP = 8;
export const ALIGN_MIDDLE = 16;
export const ALIGN_BOTTOM = 32;
export const ALIGN_BASELINE = 64;

// Browsers can't read files synchronously, so the page preloads them here keyed by basename (fonts) or path (images).
export const assets = { fonts: new Map(), images: new Map() };

export const DegToRad = deg => (deg / 180) * Math.PI;
export const RadToDeg = rad => (rad / Math.PI) * 180;

export const RGBA = (r, g, b, a) => new Float32Array([r / 255, g / 255, b / 255, a / 255]);
export const RGB = (r, g, b) => RGBA(r, g, b, 255);

const isVec = v => v !== null && typeof v == 'object';

// Same either-a-vector-or-n-scalars rule as nvgjs_arguments() in nvgjs-utils.c.
function vecArgs(list, n) {
  if(list.length >= 1 && isVec(list[0]) && list[0].length >= n) return Array.from(list[0]).slice(0, n);
  if(list.length >= n) return list.slice(0, n).map(Number);
  return null;
}

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

const setters = {
  Translate: (t, [x, y]) => t.set([1, 0, 0, 1, x, y]),
  Scale: (t, [x, y]) => t.set([x, 0, 0, y, 0, 0]),
  Rotate: (t, [a]) => t.set([Math.cos(a), Math.sin(a), -Math.sin(a), Math.cos(a), 0, 0]),
  SkewX: (t, [a]) => t.set([1, 0, Math.tan(a), 1, 0, 0]),
  SkewY: (t, [a]) => t.set([1, Math.tan(a), 0, 1, 0, 0]),
};
const arity = { Translate: 2, Scale: 2, Rotate: 1, SkewX: 1, SkewY: 1 };

export class Transform extends Float32Array {
  static get [Symbol.species]() {
    return Float32Array;
  }

  static Multiply(...args) {
    return Transform.#compose(mul, args);
  }

  static Premultiply(...args) {
    return Transform.#compose(premul, args);
  }

  static Inverse(...args) {
    const out = args.length > 1 && isVec(args[0]) ? args.shift() : new Transform(6);
    const t = args[0];
    const det = t[0] * t[3] - t[2] * t[1];
    if(det > -1e-6 && det < 1e-6) throw new Error('nvgTransformInverse failed');
    const inv = 1 / det;
    out.set([t[3] * inv, -t[1] * inv, -t[2] * inv, t[0] * inv, (t[2] * t[5] - t[3] * t[4]) * inv, (t[1] * t[4] - t[0] * t[5]) * inv]);
    return out;
  }

  static #compose(op, args) {
    const out = args.length > 1 ? args.shift() : Transform.Identity();
    while(args.length) {
      const src = vecArgs(args, 6);
      args.splice(0, isVec(args[0]) ? 1 : 6);
      op(out, src);
    }
    return out;
  }

  Multiply(...args) {
    mul(this, vecArgs(args, 6) ?? [1, 0, 0, 1, 0, 0]);
    return this;
  }

  Premultiply(...args) {
    premul(this, vecArgs(args, 6) ?? [1, 0, 0, 1, 0, 0]);
    return this;
  }

  Inverse() {
    return Transform.Inverse(this, Float32Array.from(this));
  }

  TransformPoint(...args) {
    const [x, y] = vecArgs(args, 2);
    return [x * this[0] + y * this[2] + this[4], x * this[1] + y * this[3] + this[5]];
  }
}

for(const name of Object.keys(setters)) {
  // Static form builds a fresh matrix; instance form premultiplies it onto this one.
  Transform[name] = (...args) => {
    const out = args.length > arity[name] && isVec(args[0]) ? args.shift() : new Transform(6);
    setters[name](out, vecArgs(args, arity[name]) ?? []);
    return out;
  };
  Transform.prototype[name] = function(...args) {
    const t = new Float32Array(6);
    setters[name](t, vecArgs(args, arity[name]) ?? []);
    premul(this, t);
    return this;
  };
}
Transform.Identity = () => new Transform([1, 0, 0, 1, 0, 0]);

['a', 'b', 'c', 'd', 'e', 'f'].forEach((k, i) =>
  Object.defineProperty(Transform.prototype, k, {
    get() {
      return this[i];
    },
    set(v) {
      this[i] = v;
    },
  }),
);

export function TransformPoint(dst, trf, ...src) {
  let i = 0;
  for(let n = dst.length; n >= 2; n -= 2, i++) {
    const p = src.length ? vecArgs(src, 2) : [dst[2 * i], dst[2 * i + 1]];
    if(!p) break;
    if(src.length) src.splice(0, isVec(src[0]) ? 1 : 2);
    dst[2 * i] = p[0] * trf[0] + p[1] * trf[2] + trf[4];
    dst[2 * i + 1] = p[0] * trf[1] + p[1] * trf[3] + trf[5];
  }
  return i;
}

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

const withBytes = (bytes, fn) => {
  const ptr = m._malloc(bytes.length);
  m.HEAPU8.set(bytes, ptr);
  try {
    return fn(ptr);
  } finally {
    m._free(ptr);
  }
};

const basename = path => path.slice(path.lastIndexOf('/') + 1);

class Paint {
  constructor(bytes) {
    this.bytes = bytes;
  }
}

class Context {
  BeginFrame(w, h, ratio) { m._nvgw_BeginFrame(w, h, ratio); }
  EndFrame() { m._nvgw_EndFrame(); }
  Save() { m._nvgw_Save(); }
  Restore() { m._nvgw_Restore(); }

  BeginPath() { m._nvgw_BeginPath(); }
  MoveTo(x, y) { m._nvgw_MoveTo(x, y); }
  LineTo(x, y) { m._nvgw_LineTo(x, y); }
  BezierTo(a, b, c, d, x, y) { m._nvgw_BezierTo(a, b, c, d, x, y); }
  QuadTo(cx, cy, x, y) { m._nvgw_QuadTo(cx, cy, x, y); }
  Rect(x, y, w, h) { m._nvgw_Rect(x, y, w, h); }
  RoundedRect(x, y, w, h, r) { m._nvgw_RoundedRect(x, y, w, h, r); }
  Circle(x, y, r) { m._nvgw_Circle(x, y, r); }
  Ellipse(x, y, rx, ry) { m._nvgw_Ellipse(x, y, rx, ry); }
  Fill() { m._nvgw_Fill(); }
  Stroke() { m._nvgw_Stroke(); }

  FillColor(c) { m._nvgw_FillColor(c[0], c[1], c[2], c[3] ?? 1); }
  StrokeColor(c) { m._nvgw_StrokeColor(c[0], c[1], c[2], c[3] ?? 1); }
  StrokeWidth(w) { m._nvgw_StrokeWidth(w); }

  Translate(x, y) { m._nvgw_Translate(x, y); }
  Rotate(a) { m._nvgw_Rotate(a); }
  Scale(x, y) { m._nvgw_Scale(x, y); }
  CurrentTransform() {
    const ptr = m._nvgw_CurrentTransform();
    return new Transform(m.HEAPU8.slice(ptr, ptr + 24).buffer);
  }

  ImagePattern(ox, oy, ex, ey, angle, image, alpha) {
    m._nvgw_ImagePattern(ox, oy, ex, ey, angle, image, alpha);
    const ptr = m._nvgw_PaintPtr();
    return new Paint(m.HEAPU8.slice(ptr, ptr + m._nvgw_PaintSize()));
  }

  FillPaint(paint) {
    m.HEAPU8.set(paint.bytes, m._nvgw_PaintPtr());
    m._nvgw_FillPaint();
  }

  CreateImage(path, flags) {
    const img = assets.images.get(path);
    return img ? this.CreateImageRGBA(img.width, img.height, flags, img.data) : -1;
  }

  CreateImageRGBA(w, h, flags, data) {
    const bytes = data instanceof ArrayBuffer ? new Uint8Array(data) : new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
    return withBytes(bytes, ptr => m._nvgw_CreateImageRGBA(w, h, flags, ptr));
  }

  ImageSize(id) { return [m._nvgw_ImageWidth(id), m._nvgw_ImageHeight(id)]; }

  CreateFont(name, path) {
    const data = assets.fonts.get(basename(path));
    if(!data) return -1;
    // nvgCreateFontMem takes ownership (freeData=1), so hand it a malloc'd copy.
    const bytes = new Uint8Array(data);
    const ptr = m._malloc(bytes.length);
    m.HEAPU8.set(bytes, ptr);
    return withString(name, np => m._nvgw_CreateFontMem(np, ptr, bytes.length));
  }

  FontFace(name) { withString(name, p => m._nvgw_FontFace(p)); }
  FontSize(size) { m._nvgw_FontSize(size); }
  TextAlign(align) { m._nvgw_TextAlign(align); }
  Text(x, y, str) { return withString(String(str), p => m._nvgw_Text(x, y, p)); }
}

export function CreateGL3(flags) {
  if(!m._nvgw_Create(flags)) throw new Error('WebGL2/NanoVG init failed');
  return new Context();
}

export function DeleteGL3() {
  m._nvgw_Delete();
}

const unsupported = name => () => {
  throw new Error(`${name} is not supported in the browser build`);
};
export const CreateFramebuffer = unsupported('CreateFramebuffer');
export const BindFramebuffer = unsupported('BindFramebuffer');
export const CreateImageFromHandleGL3 = unsupported('CreateImageFromHandleGL3');
export const ReadPixels = unsupported('ReadPixels');
