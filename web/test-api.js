import * as nvg from './nanovg.js';

const results = [];
const check = (name, fn) => {
  try {
    results.push([name, fn() === false ? 'FAIL' : 'ok']);
  } catch(e) {
    results.push([name, `FAIL ${e.name}: ${e.message}`]);
  }
};
const throws = (name, type, fn) =>
  check(name, () => {
    try {
      fn();
    } catch(e) {
      return e.name == type || `FAIL expected ${type}, got ${e.name}`;
    }
    return false;
  });

const font = await (await fetch('./fonts/DejaVuSans.ttf')).arrayBuffer();
nvg.writeFile('/fonts/DejaVuSans.ttf', font);

const png = await (async () => {
  const c = new OffscreenCanvas(3, 2);
  const cx = c.getContext('2d');
  cx.fillStyle = '#f00';
  cx.fillRect(0, 0, 3, 2);
  return (await c.convertToBlob({ type: 'image/png' })).arrayBuffer();
})();
nvg.writeFile('tiny.png', png);

const { CreateGL3, DeleteGL3, RGB, RGBA, Transform, Paint } = nvg;
const ctx = CreateGL3(nvg.STENCIL_STROKES | nvg.ANTIALIAS);

check('context tag', () => Object.prototype.toString.call(ctx) == '[object NVGcontext]' && ctx instanceof nvg.Context);

const red = RGB(255, 0, 0);
const pixel = (buf, w, h, x, y) => Array.from(new Uint8Array(buf, ((h - 1 - y) * w + x) * 4, 4));

check('frame + fill reads back red', () => {
  ctx.BeginFrame(400, 300, 1);
  ctx.BeginPath();
  ctx.Rect(50, 50, 100, 100);
  ctx.FillColor(red);
  ctx.Fill();
  ctx.EndFrame();
  const px = pixel(nvg.ReadPixels(400, 300), 400, 300, 100, 100);
  return px[0] > 250 && px[1] < 5 && px[2] < 5 && px[3] == 255;
});

check('state + path + paint methods', () => {
  ctx.BeginFrame(400, 300, 1);
  ctx.Save();
  ctx.Scissor(0, 0, 400, 300);
  ctx.IntersectScissor(10, 10, 300, 200);
  ctx.ResetScissor();
  ctx.MiterLimit(4);
  ctx.LineCap(nvg.ROUND);
  ctx.LineJoin(nvg.BEVEL);
  ctx.GlobalAlpha(0.9);
  ctx.ShapeAntiAlias(true);
  ctx.StrokeWidth(3);
  ctx.StrokeColor(RGBA(1, 2, 3, 4));
  ctx.StrokeColor([1, 0.5, 0.25]);
  ctx.BeginPath();
  ctx.MoveTo(10, 10);
  ctx.LineTo(50, 50);
  ctx.BezierTo(60, 60, 70, 70, 80, 80);
  ctx.QuadTo(90, 90, 100, 100);
  ctx.ArcTo(120, 100, 120, 120, 10);
  ctx.Arc(150, 150, 20, 0, Math.PI, nvg.CW);
  ctx.Arc(150, 150, 20, 0, Math.PI, nvg.CCW);
  ctx.ClosePath();
  ctx.PathWinding(nvg.HOLE);
  ctx.Rect(1, 1, 2, 2);
  ctx.RoundedRect(1, 1, 20, 20, 4);
  ctx.RoundedRectVarying(1, 1, 20, 20, 1, 2, 3, 4);
  ctx.Circle(5, 5, 3);
  ctx.Ellipse(5, 5, 3, 2);
  const lin = ctx.LinearGradient(0, 0, 10, 10, red, RGB(0, 0, 255));
  const box = ctx.BoxGradient(0, 0, 10, 10, 2, 4, red, RGB(0, 0, 255));
  const rad = ctx.RadialGradient(5, 5, 1, 5, red, RGB(0, 0, 255));
  if(!(lin instanceof Paint && box instanceof Paint && rad instanceof Paint)) return false;
  ctx.FillPaint(lin);
  ctx.StrokePaint(box);
  ctx.Fill();
  ctx.Stroke();
  ctx.Restore();
  ctx.Reset();
  ctx.EndFrame();
});

check('cancel frame', () => {
  ctx.BeginFrame(400, 300, 1);
  ctx.CancelFrame();
});

check('transforms', () => {
  ctx.BeginFrame(400, 300, 1);
  ctx.ResetTransform();
  ctx.Translate(10, 20);
  ctx.Scale(2, 3);
  const t = ctx.CurrentTransform();
  const ok = t instanceof Transform && t.a == 2 && t.d == 3 && t.e == 10 && t.f == 20;
  const out = [];
  ctx.CurrentTransform(out);
  ctx.ResetTransform();
  ctx.Transform(1, 0, 0, 1, 5, 6);
  const t2 = ctx.CurrentTransform();
  ctx.ResetTransform();
  ctx.Transform([1, 0, 0, 1, 7, 8]);
  ctx.Rotate(0.1);
  ctx.SkewX(0.1);
  ctx.SkewY(0.1);
  ctx.CancelFrame();
  return ok && out.length == 6 && out[4] == 10 && t2.e == 5 && t2.f == 6;
});

check('fonts + text', () => {
  const id = ctx.CreateFont('sans', '/fonts/DejaVuSans.ttf');
  if(id < 0 || ctx.FindFont('sans') != id || ctx.FindFont('nope') != -1) return false;
  if(ctx.CreateFont('bad', '/no/such.ttf') != -1) return false;
  if(ctx.CreateFontAtIndex('sans2', '/fonts/DejaVuSans.ttf', 0) < 0) return false;

  ctx.BeginFrame(400, 300, 1);
  ctx.FontFace('sans');
  ctx.FontSize(20);
  ctx.FontBlur(0);
  ctx.TextLetterSpacing(1);
  ctx.TextLineHeight(1.2);
  ctx.TextAlign(nvg.ALIGN_LEFT | nvg.ALIGN_BASELINE);
  ctx.FillColor(red);
  const full = ctx.Text(10, 100, 'hello wörld');
  const part = ctx.Text(10, 100, 'hello wörld', 5);
  const uni = ctx.Text(10, 100, 'ääää', 2);
  const uni2 = ctx.Text(10, 100, 'ää');
  ctx.TextBox(10, 150, 60, 'a few words that wrap around');
  const b = {};
  const adv = ctx.TextBounds(10, 100, 'hello', undefined, b);
  const bb = [];
  ctx.TextBoxBounds(10, 100, 40, 'a few words that wrap', undefined, bb);
  const b2 = ctx.TextBounds2(0, 0, 'hello');
  ctx.EndFrame();
  return full > part && part > 0 && uni == uni2 && b.xmax > b.xmin && b.ymax > b.ymin && adv > 0 && b2.width > 0 && b2.height > 0 && bb.xmax > bb.xmin;
});

check('images', () => {
  const rgba = new Uint8Array(2 * 2 * 4).fill(255).buffer;
  const id = ctx.CreateImageRGBA(2, 2, 0, rgba);
  const [w, h] = ctx.ImageSize(id);
  ctx.UpdateImage(id, new Uint8Array(16).fill(10).buffer);
  const pat = ctx.ImagePattern(0, 0, 2, 2, 0, id, 1);
  ctx.DeleteImage(id);

  const mem = ctx.CreateImageMem(0, png);
  const [mw, mh] = ctx.ImageSize(mem);
  const file = ctx.CreateImage('tiny.png', 0);
  const [fw, fh] = ctx.ImageSize(file);
  const missing = ctx.CreateImage('missing.png', 0);
  return id > 0 && w == 2 && h == 2 && pat instanceof Paint && mem > 0 && mw == 3 && mh == 2 && file > 0 && fw == 3 && fh == 2 && missing == 0;
});

check('framebuffer render + read', () => {
  const fb = nvg.CreateFramebuffer(ctx, 64, 64, 0);
  if(!(fb.fbo > 0 && fb.texture > 0 && fb.image > 0) || Object.prototype.toString.call(fb) != '[object NVGLUframebuffer]') return false;
  nvg.BindFramebuffer(fb);
  ctx.BeginFrame(64, 64, 1);
  ctx.BeginPath();
  ctx.Rect(0, 0, 64, 64);
  ctx.FillColor(RGB(0, 255, 0));
  ctx.Fill();
  ctx.EndFrame();
  const px = pixel(nvg.ReadPixels(64, 64), 64, 64, 32, 32);
  const texture = fb.texture;
  const handle = nvg.ImageHandleGL3(ctx, fb.image);
  const img = nvg.CreateImageFromHandleGL3(ctx, texture, 64, 64, nvg.IMAGE_NODELETE);
  nvg.BindFramebuffer(null);
  nvg.DeleteFramebuffer(fb);
  return px[1] > 250 && px[0] < 5 && handle == texture && img > 0;
});
throws('deleted framebuffer throws', 'TypeError', () => {
  const fb = nvg.CreateFramebuffer(ctx, 8, 8, 0);
  nvg.DeleteFramebuffer(fb);
  fb.fbo;
});
throws('bind undefined throws', 'TypeError', () => nvg.BindFramebuffer(undefined));

check('second context', () => {
  const ctx2 = CreateGL3(0);
  ctx2.BeginFrame(400, 300, 1);
  ctx2.BeginPath();
  ctx2.Rect(0, 0, 10, 10);
  ctx2.FillColor(red);
  ctx2.Fill();
  ctx2.EndFrame();
  DeleteGL3(ctx2);
  ctx.Save();
  ctx.Restore();
});
throws('deleted context throws', 'TypeError', () => {
  const ctx3 = CreateGL3(0);
  DeleteGL3(ctx3);
  ctx3.Save();
});
throws('argument count', 'InternalError', () => ctx.Rect(1, 2, 3));
throws('paint type', 'TypeError', () => ctx.FillPaint({}));

const failed = results.filter(([, r]) => r != 'ok').length;
document.title = failed ? `FAIL ${failed}` : `PASS ${results.length}`;
document.getElementById('out').textContent = results.map(([n, r]) => `${r == 'ok' ? 'ok  ' : 'FAIL'} ${n}${r == 'ok' ? '' : ` -- ${r}`}`).join('\n');
