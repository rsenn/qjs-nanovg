// Runs unchanged under qjsm (against nanovg.so) and under Node/browsers (against nanovg.js); compat-diff.js compares the JSON.
const CLASSES = ['Context', 'Color', 'Transform', 'Paint'];

const flags = d => (d.writable ? 'w' : '') + (d.configurable ? 'c' : '') + (d.enumerable ? 'e' : '');

const members = (o, skip = []) =>
  Object.fromEntries(
    Object.getOwnPropertyNames(o ?? {})
      .filter(k => !skip.includes(k))
      .sort()
      .map(k => {
        const d = Object.getOwnPropertyDescriptor(o, k);
        return [k, 'value' in d ? (typeof d.value == 'function' ? `fn/${d.value.length}/${d.value.name}/${flags(d)}` : `${typeof d.value}/${flags(d)}`) : `accessor/${flags(d)}`];
      }),
  );

export function describe(ns) {
  const exports = {};
  for(const k of Object.keys(ns).sort()) {
    const v = ns[k];
    exports[k] = CLASSES.includes(k) ? 'class' : typeof v == 'function' ? `fn/${v.length}/${v.name}` : `${typeof v} ${v}`;
  }

  const tproto = Object.getPrototypeOf(ns.Transform.Identity());
  const cproto = Object.getPrototypeOf(ns.RGB(1, 2, 3));

  return {
    exports,
    Context: { statics: members(ns.Context, ['length', 'name', 'prototype']), proto: members(ns.Context.prototype, ['constructor']), tag: ns.Context.prototype[Symbol.toStringTag] },
    Color: { proto: members(cproto, ['constructor']), tag: cproto[Symbol.toStringTag] },
    Transform: { statics: members(ns.Transform, ['length', 'name', 'prototype']), proto: members(tproto, ['constructor']), tag: tproto[Symbol.toStringTag] },
  };
}

const arr = v => (v === undefined ? 'undefined' : Array.from(v));
const err = f => {
  try {
    f();
    return 'no error';
  } catch(e) {
    return `${e.name}: ${e.message}`;
  }
};

export function values(ns) {
  const { Transform: T, Context, RGB, RGBA, RGBf, RGBAf, LerpRGBA, TransRGBA, TransRGBAf, HSL, HSLA, DegToRad, RadToDeg, TransformPoint } = ns;
  const out = {};

  out.deg = [0, 45, 90, 180, 360, -30, 1e-3].map(d => [DegToRad(d), RadToDeg(DegToRad(d))]);
  out.rgb = [0, 1, 127, 128, 254, 255, 256, -1, 300, 3.7].map(r => [arr(RGB(r, 255 - (r & 255), 7)), arr(RGBA(r, 9, 200, r + 5))]);
  out.rgbf = [arr(RGBf(0.1, 0.2, 0.3)), arr(RGBAf(0.1, 0.2, 0.3, 0.4))];
  out.lerp = [-1, 0, 0.25, 1, 2].map(u => arr(LerpRGBA(RGBA(10, 20, 30, 40), RGBAf(1, 0.5, 0.25, 0.125), u)));
  out.trans = [arr(TransRGBA(RGB(10, 20, 30), 128)), arr(TransRGBAf(RGB(10, 20, 30), 0.3)), arr(TransRGBA([0.1, 0.2, 0.3], 255))];
  out.hsl = [];
  for(const h of [-0.25, 0, 0.1, 0.5, 0.9, 1.3])
    for(const s of [0, 0.5, 1]) for(const l of [0, 0.3, 0.5, 0.8, 1]) out.hsl.push(arr(HSL(h, s, l)), arr(HSLA(h, s, l, 100)));

  const c = RGB(1, 2, 3);
  c.r = 0.5;
  out.color = [c instanceof Float32Array, Object.prototype.toString.call(c), c.r, c.a, c.length];

  out.static = [arr(T.Translate(3, 4)), arr(T.Scale(2, 3)), arr(T.Rotate(0.5)), arr(T.SkewX(0.3)), arr(T.SkewY(0.2)), arr(T.Identity()), arr(T.Translate([5, 6])), arr(T.Scale([2, 3]))];

  const o = new Float32Array(6);
  const a = [0, 0, 0, 0, 0, 0];
  out.inout = [String(T.Translate(o, 7, 8)), arr(o), String(T.Scale(o, [2, 3])), arr(o), String(T.Rotate(a, 1)), arr(a), String(T.Identity(o)), arr(o)];

  const m1 = T.Translate(1, 2);
  const m2 = T.Scale(3, 3);
  const r = T.Multiply(m1, m2);
  out.multiply = [r === m1, arr(m1), arr(T.Multiply(T.Translate(1, 2), T.Scale(3, 3), T.Rotate(0.4)))];
  const p1 = T.Translate(1, 2);
  out.premultiply = [T.Premultiply(p1, T.Scale(3, 3)) === p1, arr(p1)];
  const single = T.Translate(1, 2);
  out.single = [String(T.Multiply(single)), arr(single)];
  const inv = new Float32Array(6);
  out.inverse = [String(T.Inverse(inv, T.Scale(2, 4))), arr(inv), arr(inv)];
  const inv2 = T.Inverse(inv, T.Rotate(0.7));
  out.inverse2 = [inv2 === inv, arr(inv)];

  const chain = T.Scale(2, 3).Rotate(0.5).Translate(1, 1);
  out.chain = [arr(chain), arr(T.Scale(2, 3).Scale(2)), arr(T.Translate(1, 2).Multiply(T.Scale(2, 3))), arr(T.Translate(1, 2).Premultiply(T.Scale(2, 3))), arr(T.Scale(2, 4).Inverse()), arr(T.Scale(2, 3).SkewX(0.1).SkewY(0.2))];
  const tp = T.Translate(10, 20).Scale(2, 2);
  out.point = [tp.TransformPoint(1, 2), tp.TransformPoint([3, 4])];

  const g = T.Translate(1, 2);
  g.e = 9;
  g.xx = 4;
  out.getters = [g.a, g.b, g.c, g.d, g.e, g.f, g.xx, g.yx, g.xy, g.yy, g.x0, g.y0, Object.prototype.toString.call(g)];

  const dst = new Float32Array([1, 2, 3, 4]);
  out.transformPoint = [TransformPoint(dst, T.Translate(10, 20)), arr(dst)];
  const dst2 = new Float32Array(4);
  out.transformPoint2 = [TransformPoint(dst2, T.Scale(2, 2), 5, 6, [7, 8]), arr(dst2)];

  const lone = () => Context.prototype.BeginFrame.call({}, 1, 2, 3);
  out.errors = {
    rgb: err(() => RGB(1)),
    rgba: err(() => RGBA(1, 2, 3)),
    hsl: err(() => HSL(1)),
    deg: err(() => DegToRad()),
    translate: err(() => T.Translate()),
    scale: err(() => T.Scale(2)),
    rotate: err(() => T.Rotate()),
    multiply: err(() => T.Multiply()),
    inverse: err(() => T.Inverse(new Float32Array(6))),
    singular: err(() => T.Inverse(new Float32Array(6), new Float32Array(6))),
    point: err(() => TransformPoint(1)),
    context: err(lone),
    lerp: err(() => LerpRGBA([1], [1, 2, 3], 0.5)),
  };

  return out;
}

export const collect = ns => ({ describe: describe(ns), values: values(ns) });
