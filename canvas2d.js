/*
 * canvas2d.js — a thin CanvasRenderingContext2D emulation for qjsm, built on
 * the 'nanovg', 'glfw' and 'dom' native/JS modules.
 *
 * Importing this file (for side effects) sets up a `window`/`document` with
 * one `<canvas id="canvas">` in `document.body`, backed by a real GLFW window
 * and a NanoVG GL3 context. Scripts written against the browser Canvas2D API
 * (window.requestAnimationFrame, canvas.getContext('2d'), drawImage, etc.)
 * can then run mostly unmodified.
 *
 * Usage:
 *   import 'nanovg/canvas2d.js';       // or a relative path
 *   const canvas = document.getElementById('canvas');
 *   const ctx = canvas.getContext('2d');
 *   function loop(t) {
 *     ctx.clearRect(0, 0, canvas.width, canvas.height);
 *     ctx.fillRect(10, 10, 100, 100);
 *     requestAnimationFrame(loop);
 *   }
 *   requestAnimationFrame(loop);
 *
 * KNOWN GAPS (this is a *thin* emulation, not a pixel-perfect one):
 *  - The drawing surface is a plain double-buffered GL window, not a
 *    persistent raster surface: content is NOT retained between frames.
 *    Redraw the whole canvas every frame (as most games already do).
 *  - clearRect() only paints a rect with the current "clear" color; it does
 *    not make pixels transparent (the nanovg binding exposes no blend-mode /
 *    composite-op control — see BUGS).
 *  - clip() only supports a path that is a single rect()/roundRect() call
 *    (mapped to nvg's rectangular Scissor); arbitrary clip shapes are not
 *    supported.
 *  - Gradients support only the first/last color stop (nvg gradients are
 *    2-color); intermediate addColorStop() calls are ignored.
 *  - ellipse()/arc() with a non-full sweep or rotation is approximated via a
 *    temporary transform, not native elliptical-arc math.
 *  - strokeText() falls back to filled text using strokeStyle as the color.
 *  - globalCompositeOperation, imageSmoothingEnabled and line dashing are
 *    accepted but have no effect (unsupported by the underlying binding).
 *  - Image loading (`new Image(); img.src = path`) is synchronous (reads
 *    from local disk via NanoVG's image loader); onload fires immediately.
 *  - Only local file paths are supported for image/font sources, no network.
 */

import * as os from 'os';
import * as glfw from 'glfw';
import * as nvg from 'nanovg';
import { Factory, Node } from 'dom';

const DEFAULT_WIDTH = 800;
const DEFAULT_HEIGHT = 600;
const FRAME_MS = 1000 / 60;

/* ---------------------------------------------------------------- events */

function makeEvent(type, target, props = {}) {
  return Object.assign(
    {
      type,
      target,
      currentTarget: target,
      timeStamp: nowMs(),
      defaultPrevented: false,
      preventDefault() {
        this.defaultPrevented = true;
      },
      stopPropagation() {},
      stopImmediatePropagation() {},
    },
    props,
  );
}

function makeEventTarget(obj) {
  const listeners = new Map();
  obj.addEventListener = (type, fn) => {
    if(!fn) return;
    if(!listeners.has(type)) listeners.set(type, new Set());
    listeners.get(type).add(fn);
  };
  obj.removeEventListener = (type, fn) => listeners.get(type)?.delete(fn);
  obj.dispatchEvent = evt => {
    evt.target ??= obj;
    evt.currentTarget = obj;
    for(const fn of listeners.get(evt.type) ?? []) {
      try {
        (typeof fn == 'function' ? fn : fn.handleEvent.bind(fn))(evt);
      } catch(e) {
        reportError(e);
      }
    }
    const prop = obj['on' + evt.type];
    if(typeof prop == 'function') {
      try {
        prop.call(obj, evt);
      } catch(e) {
        reportError(e);
      }
    }
    return !evt.defaultPrevented;
  };
  return obj;
}

function reportError(e) {
  const msg = (e && e.stack) || String(e);
  if(globalThis.console) globalThis.console.error('canvas2d:', msg);
}

/* ---------------------------------------------------------------- timing */

function nowMs() {
  return glfw.getTime() * 1000;
}

const performance = { now: nowMs };

const timers = new Map();
let timerId = 1;

function setTimeout(fn, ms = 0, ...args) {
  const id = timerId++;
  const handle = os.setTimeout(() => {
    timers.delete(id);
    fn(...args);
  }, Math.max(0, ms | 0));
  timers.set(id, handle);
  return id;
}

function clearTimeout(id) {
  const handle = timers.get(id);
  if(handle !== undefined) {
    os.clearTimeout(handle);
    timers.delete(id);
  }
}

function setInterval(fn, ms = 0, ...args) {
  const id = timerId++;
  const tick = () => {
    if(!timers.has(id)) return;
    const handle = os.setTimeout(tick, Math.max(1, ms | 0));
    timers.set(id, handle);
    fn(...args);
  };
  const handle = os.setTimeout(tick, Math.max(1, ms | 0));
  timers.set(id, handle);
  return id;
}

const clearInterval = clearTimeout;

/* ------------------------------------------------------------- document */

const factory = new Factory();
const document = factory.Document.new({ tagName: '#document', attributes: {}, children: [] }, factory);
Factory.set(document, factory);

const htmlEl = document.createElement('html');
const headEl = document.createElement('head');
const bodyEl = document.createElement('body');
document.appendChild(htmlEl);
htmlEl.appendChild(headEl);
htmlEl.appendChild(bodyEl);

function findById(node, id) {
  if(node.getAttribute && node.getAttribute('id') === id) return node;
  for(const child of node.children ?? []) {
    const found = findById(child, id);
    if(found) return found;
  }
  return null;
}

document.readyState = 'complete';
document.title = '';

makeEventTarget(document);

/* ------------------------------------------------------------- window(s) */

const title = (typeof scriptArgs != 'undefined' && scriptArgs[1]) || 'canvas2d';

glfw.Window.hint(glfw.CONTEXT_VERSION_MAJOR, 3);
glfw.Window.hint(glfw.CONTEXT_VERSION_MINOR, 2);
glfw.Window.hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE);
glfw.Window.hint(glfw.OPENGL_FORWARD_COMPAT, true);
glfw.Window.hint(glfw.SAMPLES, 4);

const glfwWindow = (glfw.context.current = new glfw.Window(DEFAULT_WIDTH, DEFAULT_HEIGHT, title));
const nv = nvg.CreateGL3(nvg.STENCIL_STROKES | nvg.ANTIALIAS);

const window = {};
makeEventTarget(window);
Object.assign(window, {
  document,
  innerWidth: DEFAULT_WIDTH,
  innerHeight: DEFAULT_HEIGHT,
  devicePixelRatio: 1,
  performance,
  requestAnimationFrame,
  cancelAnimationFrame,
  setTimeout,
  clearTimeout,
  setInterval,
  clearInterval,
  console: globalThis.console,
  navigator: { userAgent: 'qjsm-canvas2d', platform: 'native' },
  location: { href: '', reload() {} },
  alert(msg) {
    (globalThis.console ?? { log: () => {} }).log(String(msg));
  },
  close() {
    glfwWindow.shouldClose = true;
  },
});

/* -------------------------------------------------------------- <canvas> */

const MOUSE_BUTTON_MAP = [0, 2, 1]; // GLFW LEFT,RIGHT,MIDDLE -> DOM 0,2,1
const KEY_NAME_MAP = {
  [glfw.KEY_ESCAPE]: 'Escape',
  [glfw.KEY_ENTER]: 'Enter',
  [glfw.KEY_TAB]: 'Tab',
  [glfw.KEY_BACKSPACE]: 'Backspace',
  [glfw.KEY_SPACE]: ' ',
  [glfw.KEY_LEFT]: 'ArrowLeft',
  [glfw.KEY_RIGHT]: 'ArrowRight',
  [glfw.KEY_UP]: 'ArrowUp',
  [glfw.KEY_DOWN]: 'ArrowDown',
  [glfw.KEY_LEFT_SHIFT]: 'Shift',
  [glfw.KEY_RIGHT_SHIFT]: 'Shift',
  [glfw.KEY_LEFT_CONTROL]: 'Control',
  [glfw.KEY_RIGHT_CONTROL]: 'Control',
  [glfw.KEY_LEFT_ALT]: 'Alt',
  [glfw.KEY_RIGHT_ALT]: 'Alt',
};

function keyName(key) {
  if(key in KEY_NAME_MAP) return KEY_NAME_MAP[key];
  if(key >= 32 && key < 127) return String.fromCharCode(key).toLowerCase();
  return 'Unidentified';
}

const canvas = document.createElement('canvas');
canvas.setAttribute('id', 'canvas');
canvas.setAttribute('width', String(DEFAULT_WIDTH));
canvas.setAttribute('height', String(DEFAULT_HEIGHT));
bodyEl.appendChild(canvas);

let canvasWidth = DEFAULT_WIDTH;
let canvasHeight = DEFAULT_HEIGHT;
let ctx2dInstance = null;
let resizingFromWindow = false;

function applyWindowSize() {
  if(resizingFromWindow) return;
  const s = glfwWindow.size;
  s.width = canvasWidth;
  s.height = canvasHeight;
  glfwWindow.size = s;
}

Object.defineProperty(canvas, 'width', {
  configurable: true,
  get: () => canvasWidth,
  set(v) {
    canvasWidth = v | 0;
    canvas.setAttribute('width', String(canvasWidth));
    applyWindowSize();
  },
});
Object.defineProperty(canvas, 'height', {
  configurable: true,
  get: () => canvasHeight,
  set(v) {
    canvasHeight = v | 0;
    canvas.setAttribute('height', String(canvasHeight));
    applyWindowSize();
  },
});
Object.defineProperty(canvas, 'clientWidth', { get: () => canvasWidth });
Object.defineProperty(canvas, 'clientHeight', { get: () => canvasHeight });

canvas.getBoundingClientRect = () => ({ x: 0, y: 0, left: 0, top: 0, width: canvasWidth, height: canvasHeight, right: canvasWidth, bottom: canvasHeight });
canvas.getContext = type => {
  if(type != '2d') return null;
  return (ctx2dInstance ??= new CanvasRenderingContext2D(canvas));
};
canvas.toDataURL = () => {
  throw new Error('canvas.toDataURL() is not supported by this emulation');
};
makeEventTarget(canvas);

applyWindowSize();

// dom.js's tree-traversal path (NodeList/children/querySelector) mints a
// fresh Element wrapper per access instead of reusing Element.cache (see
// BUGS: "dom-getnode-bypasses-element-cache"), so lookups would otherwise
// return an object without our getContext/width/height/event patches.
// Normalize any node whose raw data is the canvas back to the singleton.
const canvasRaw = Node.raw(canvas);
function normalizeCanvas(node) {
  return node && Node.raw(node) === canvasRaw ? canvas : node;
}

document.getElementById = id => normalizeCanvas(findById(document, id));

const rawQuerySelector = document.querySelector.bind(document);
document.querySelector = selector => normalizeCanvas(rawQuerySelector(selector));

const rawQuerySelectorAll = document.querySelectorAll.bind(document);
document.querySelectorAll = function* (selector) {
  for(const node of rawQuerySelectorAll(selector)) yield normalizeCanvas(node);
};

glfwWindow.handleSize = (w, h) => {
  resizingFromWindow = true;
  canvas.width = w;
  canvas.height = h;
  resizingFromWindow = false;
  window.innerWidth = w;
  window.innerHeight = h;
  window.dispatchEvent(makeEvent('resize', window));
};

glfwWindow.handleKey = (key, scancode, action, mods) => {
  const type = action === 0 ? 'keyup' : 'keydown';
  const evt = makeEvent(type, canvas, {
    key: keyName(key),
    code: keyName(key),
    keyCode: key,
    which: key,
    repeat: action === 2,
    shiftKey: !!(mods & 0x0001),
    ctrlKey: !!(mods & 0x0002),
    altKey: !!(mods & 0x0004),
    metaKey: !!(mods & 0x0008),
  });
  canvas.dispatchEvent(evt);
  window.dispatchEvent(evt);
};

glfwWindow.handleChar = code => {
  window.dispatchEvent(makeEvent('keypress', canvas, { charCode: code, key: String.fromCodePoint(code) }));
};

glfwWindow.handleMouseButton = (button, action, mods) => {
  const pos = glfwWindow.cursorPos;
  const type = action === 0 ? 'mouseup' : 'mousedown';
  const evt = makeEvent(type, canvas, { button: MOUSE_BUTTON_MAP[button] ?? button, clientX: pos.x, clientY: pos.y, offsetX: pos.x, offsetY: pos.y });
  canvas.dispatchEvent(evt);
  if(type == 'mousedown') canvas.dispatchEvent(makeEvent('click', canvas, { button: evt.button, clientX: pos.x, clientY: pos.y }));
};

glfwWindow.handleCursorPos = (x, y) => {
  canvas.dispatchEvent(makeEvent('mousemove', canvas, { clientX: x, clientY: y, offsetX: x, offsetY: y }));
};

glfwWindow.handleScroll = (xoff, yoff) => {
  canvas.dispatchEvent(makeEvent('wheel', canvas, { deltaX: -xoff * 100, deltaY: -yoff * 100, deltaMode: 0 }));
};

glfwWindow.handleClose = () => {
  window.dispatchEvent(makeEvent('close', window));
};

/* ---------------------------------------------------------------- fonts */

const FONT_DIR = '/usr/share/fonts/truetype/dejavu/';
const FONT_FILES = {
  'sans-serif': FONT_DIR + 'DejaVuSans.ttf',
  'sans-serif-bold': FONT_DIR + 'DejaVuSans-Bold.ttf',
  'sans-serif-italic': FONT_DIR + 'DejaVuSans-Oblique.ttf',
  serif: FONT_DIR + 'DejaVuSerif.ttf',
  'serif-bold': FONT_DIR + 'DejaVuSerif-Bold.ttf',
  monospace: FONT_DIR + 'DejaVuSansMono.ttf',
  'monospace-bold': FONT_DIR + 'DejaVuSansMono-Bold.ttf',
};

const loadedFonts = new Set();

function ensureFont(name) {
  if(loadedFonts.has(name)) return name;
  const file = FONT_FILES[name] ?? FONT_FILES['sans-serif'];
  if(nv.CreateFont(name, file) === -1) {
    reportError(new Error(`could not load font '${name}' from ${file}`));
    return name == 'sans-serif' ? name : ensureFont('sans-serif');
  }
  loadedFonts.add(name);
  return name;
}

/**
 * registerFont(family, path) — load a custom .ttf under a family name usable
 * from ctx.font, e.g. registerFont('title', 'assets/Title.ttf'); ctx.font =
 * '32px title';
 */
function registerFont(family, path) {
  if(nv.CreateFont(family, path) === -1) throw new Error(`could not load font '${family}' from ${path}`);
  loadedFonts.add(family);
}

const FONT_RE = /^\s*(italic\s+)?(bold\s+)?([\d.]+)px\s+([\w-]+)/i;

function parseFont(str) {
  const m = FONT_RE.exec(str);
  if(!m) return { size: 16, family: 'sans-serif' };
  const [, italic, bold, size, family] = m;
  let key = family.toLowerCase().replace(/["']/g, '');
  if(!(key in FONT_FILES) && !loadedFonts.has(key)) key = 'sans-serif';
  if(bold && FONT_FILES[key + '-bold']) key += '-bold';
  else if(italic && FONT_FILES[key + '-italic']) key += '-italic';
  return { size: parseFloat(size), family: key };
}

/* ---------------------------------------------------------------- color */

const NAMED_COLORS = {
  black: '#000000',
  white: '#ffffff',
  red: '#ff0000',
  green: '#008000',
  lime: '#00ff00',
  blue: '#0000ff',
  yellow: '#ffff00',
  cyan: '#00ffff',
  aqua: '#00ffff',
  magenta: '#ff00ff',
  fuchsia: '#ff00ff',
  gray: '#808080',
  grey: '#808080',
  orange: '#ffa500',
  purple: '#800080',
  pink: '#ffc0cb',
  brown: '#a52a2a',
  navy: '#000080',
  teal: '#008080',
  olive: '#808000',
  maroon: '#800000',
  silver: '#c0c0c0',
  gold: '#ffd700',
  indigo: '#4b0082',
  violet: '#ee82ee',
  transparent: 'rgba(0,0,0,0)',
};

function parseColor(value) {
  if(value instanceof CanvasGradient || value instanceof CanvasPattern) return value;
  if(typeof value != 'string') return nvg.RGBA(0, 0, 0, 255);
  let s = value.trim().toLowerCase();
  if(s in NAMED_COLORS) s = NAMED_COLORS[s];
  let m;
  if((m = /^#([0-9a-f]{3})$/.exec(s))) {
    const [r, g, b] = m[1].split('').map(c => parseInt(c + c, 16));
    return nvg.RGBA(r, g, b, 255);
  }
  if((m = /^#([0-9a-f]{6})$/.exec(s))) {
    const n = parseInt(m[1], 16);
    return nvg.RGBA((n >> 16) & 255, (n >> 8) & 255, n & 255, 255);
  }
  if((m = /^#([0-9a-f]{8})$/.exec(s))) {
    const n = parseInt(m[1], 16);
    return nvg.RGBA((n >>> 24) & 255, (n >> 16) & 255, (n >> 8) & 255, n & 255);
  }
  if((m = /^rgba?\(([^)]+)\)$/.exec(s))) {
    const p = m[1].split(',').map(x => x.trim());
    const a = p[3] !== undefined ? Math.round(parseFloat(p[3]) * 255) : 255;
    return nvg.RGBA(parseFloat(p[0]), parseFloat(p[1]), parseFloat(p[2]), a);
  }
  return nvg.RGBA(0, 0, 0, 255);
}

/* ----------------------------------------------------- gradients/pattern */

class CanvasGradient {
  constructor(kind, coords) {
    this._kind = kind;
    this._coords = coords;
    this._stops = [];
  }

  addColorStop(offset, color) {
    this._stops.push([offset, parseColor(color)]);
  }

  _paint() {
    if(!this._stops.length) return parseColor('black');
    const sorted = [...this._stops].sort((a, b) => a[0] - b[0]);
    const icol = sorted[0][1];
    const ocol = sorted[sorted.length - 1][1];
    if(this._kind == 'linear') {
      const [x0, y0, x1, y1] = this._coords;
      return nv.LinearGradient(x0, y0, x1, y1, icol, ocol);
    }
    const [, , r0, x1, y1, r1] = this._coords;
    return nv.RadialGradient(x1, y1, r0, r1, icol, ocol);
  }
}

class CanvasPattern {
  constructor(image) {
    this._image = image;
  }

  _paint() {
    const { width: w, height: h, _id } = this._image;
    return nv.ImagePattern(0, 0, w, h, 0, _id, 1);
  }
}

/* ------------------------------------------------------------ HTMLImage */

class Image {
  constructor(width, height) {
    this._w = width ?? 0;
    this._h = height ?? 0;
    this._id = -1;
    this._src = '';
    this.onload = null;
    this.onerror = null;
  }

  get width() {
    return this._w;
  }
  set width(v) {
    this._w = v;
  }
  get height() {
    return this._h;
  }
  set height(v) {
    this._h = v;
  }
  get naturalWidth() {
    return this._w;
  }
  get naturalHeight() {
    return this._h;
  }
  get complete() {
    return this._id >= 0;
  }
  get src() {
    return this._src;
  }

  set src(path) {
    this._src = path;
    try {
      const id = nv.CreateImage(path, 0);
      if(id <= 0) throw new Error(`failed to load image '${path}'`);
      const [w, h] = nv.ImageSize(id);
      this._id = id;
      this._w = w;
      this._h = h;
      this.onload?.(makeEvent('load', this));
    } catch(e) {
      this.onerror?.(makeEvent('error', this, { error: e }));
    }
  }
}

/* -------------------------------------------------- CanvasRenderingContext2D */

const DEFAULT_STATE = () => ({
  fillStyle: '#000000',
  strokeStyle: '#000000',
  font: '10px sans-serif',
  textAlign: 'start',
  textBaseline: 'alphabetic',
  globalAlpha: 1,
  globalCompositeOperation: 'source-over',
  lineWidth: 1,
  lineCap: 'butt',
  lineJoin: 'miter',
  miterLimit: 10,
  imageSmoothingEnabled: true,
});

const H_ALIGN = { start: nvg.ALIGN_LEFT, left: nvg.ALIGN_LEFT, end: nvg.ALIGN_RIGHT, right: nvg.ALIGN_RIGHT, center: nvg.ALIGN_CENTER };
const V_ALIGN = { top: nvg.ALIGN_TOP, hanging: nvg.ALIGN_TOP, middle: nvg.ALIGN_MIDDLE, alphabetic: nvg.ALIGN_BASELINE, ideographic: nvg.ALIGN_BASELINE, bottom: nvg.ALIGN_BOTTOM };
const LINE_CAP = { butt: nvg.BUTT, round: nvg.ROUND, square: nvg.SQUARE };
const LINE_JOIN = { miter: nvg.MITER, round: nvg.ROUND, bevel: nvg.BEVEL };

class CanvasRenderingContext2D {
  constructor(canvas) {
    this.canvas = canvas;
    this._state = DEFAULT_STATE();
    this._stack = [];
    this._lastRect = null;
    this._applyFont();
    this._applyTextAlign();
  }

  /* -- state -- */

  save() {
    nv.Save();
    this._stack.push({ ...this._state });
  }

  restore() {
    nv.Restore();
    const s = this._stack.pop();
    if(s) this._state = s;
  }

  get globalAlpha() {
    return this._state.globalAlpha;
  }
  set globalAlpha(v) {
    this._state.globalAlpha = v;
    nv.GlobalAlpha(v);
  }

  get globalCompositeOperation() {
    return this._state.globalCompositeOperation;
  }
  set globalCompositeOperation(v) {
    this._state.globalCompositeOperation = v; // unsupported by the nanovg binding; stored only
  }

  get imageSmoothingEnabled() {
    return this._state.imageSmoothingEnabled;
  }
  set imageSmoothingEnabled(v) {
    this._state.imageSmoothingEnabled = v; // unsupported per-draw by nanovg; stored only
  }

  get lineWidth() {
    return this._state.lineWidth;
  }
  set lineWidth(v) {
    this._state.lineWidth = v;
    nv.StrokeWidth(v);
  }

  get lineCap() {
    return this._state.lineCap;
  }
  set lineCap(v) {
    this._state.lineCap = v;
    nv.LineCap(LINE_CAP[v] ?? nvg.BUTT);
  }

  get lineJoin() {
    return this._state.lineJoin;
  }
  set lineJoin(v) {
    this._state.lineJoin = v;
    nv.LineJoin(LINE_JOIN[v] ?? nvg.MITER);
  }

  get miterLimit() {
    return this._state.miterLimit;
  }
  set miterLimit(v) {
    this._state.miterLimit = v;
    nv.MiterLimit(v);
  }

  get fillStyle() {
    return this._state.fillStyle;
  }
  set fillStyle(v) {
    this._state.fillStyle = v;
  }

  get strokeStyle() {
    return this._state.strokeStyle;
  }
  set strokeStyle(v) {
    this._state.strokeStyle = v;
  }

  get font() {
    return this._state.font;
  }
  set font(v) {
    this._state.font = v;
    this._applyFont();
  }

  _applyFont() {
    const { size, family } = parseFont(this._state.font);
    ensureFont(family);
    nv.FontSize(size);
    nv.FontFace(family);
  }

  get textAlign() {
    return this._state.textAlign;
  }
  set textAlign(v) {
    this._state.textAlign = v;
    this._applyTextAlign();
  }

  get textBaseline() {
    return this._state.textBaseline;
  }
  set textBaseline(v) {
    this._state.textBaseline = v;
    this._applyTextAlign();
  }

  _applyTextAlign() {
    const h = H_ALIGN[this._state.textAlign] ?? nvg.ALIGN_LEFT;
    const v = V_ALIGN[this._state.textBaseline] ?? nvg.ALIGN_BASELINE;
    nv.TextAlign(h | v);
  }

  _applyFillStyle() {
    const v = this._state.fillStyle;
    if(v instanceof CanvasGradient || v instanceof CanvasPattern) nv.FillPaint(v._paint());
    else nv.FillColor(parseColor(v));
  }

  _applyStrokeStyle() {
    const v = this._state.strokeStyle;
    if(v instanceof CanvasGradient || v instanceof CanvasPattern) nv.StrokePaint(v._paint());
    else nv.StrokeColor(parseColor(v));
  }

  /* -- transforms -- */

  translate(x, y) {
    nv.Translate(x, y);
  }
  rotate(a) {
    nv.Rotate(a);
  }
  scale(x, y) {
    nv.Scale(x, y);
  }
  transform(a, b, c, d, e, f) {
    nv.Transform(a, b, c, d, e, f);
  }
  setTransform(a, b, c, d, e, f) {
    if(typeof a == 'object' && a) ({ a, b, c, d, e, f } = a);
    nv.ResetTransform();
    nv.Transform(a, b, c, d, e, f);
  }
  resetTransform() {
    nv.ResetTransform();
  }
  getTransform() {
    const [a, b, c, d, e, f] = nv.CurrentTransform();
    return { a, b, c, d, e, f, is2D: true, isIdentity: a == 1 && b == 0 && c == 0 && d == 1 && e == 0 && f == 0 };
  }

  /* -- path construction -- */

  beginPath() {
    this._lastRect = null;
    nv.BeginPath();
  }
  closePath() {
    nv.ClosePath();
  }
  moveTo(x, y) {
    this._lastRect = null;
    nv.MoveTo(x, y);
  }
  lineTo(x, y) {
    this._lastRect = null;
    nv.LineTo(x, y);
  }
  bezierCurveTo(c1x, c1y, c2x, c2y, x, y) {
    this._lastRect = null;
    nv.BezierTo(c1x, c1y, c2x, c2y, x, y);
  }
  quadraticCurveTo(cx, cy, x, y) {
    this._lastRect = null;
    nv.QuadTo(cx, cy, x, y);
  }
  arcTo(x1, y1, x2, y2, radius) {
    this._lastRect = null;
    nv.ArcTo(x1, y1, x2, y2, radius);
  }

  arc(x, y, r, startAngle, endAngle, ccw = false) {
    this._lastRect = null;
    nv.Arc(x, y, r, startAngle, endAngle, ccw ? nvg.CCW : nvg.CW);
  }

  ellipse(x, y, rx, ry, rotation = 0, startAngle = 0, endAngle = 2 * Math.PI, ccw = false) {
    this._lastRect = null;
    const full = Math.abs(endAngle - startAngle) >= 2 * Math.PI - 1e-6;
    if(rotation === 0 && startAngle === 0 && full) {
      nv.Ellipse(x, y, rx, ry);
      return;
    }
    nv.Save();
    nv.Translate(x, y);
    nv.Rotate(rotation);
    nv.Scale(rx, ry);
    nv.Arc(0, 0, 1, startAngle, endAngle, ccw ? nvg.CCW : nvg.CW);
    nv.Restore();
  }

  rect(x, y, w, h) {
    this._lastRect = [x, y, w, h];
    nv.Rect(x, y, w, h);
  }

  roundRect(x, y, w, h, radii = 0) {
    this._lastRect = null;
    let tl, tr, br, bl;
    if(Array.isArray(radii)) [tl = 0, tr = tl, br = tl, bl = tr] = radii;
    else tl = tr = br = bl = radii;
    nv.RoundedRectVarying(x, y, w, h, tl, tr, br, bl);
  }

  /* -- filling/stroking -- */

  fill() {
    this._applyFillStyle();
    nv.Fill();
  }

  stroke() {
    this._applyStrokeStyle();
    nv.Stroke();
  }

  clip() {
    if(this._lastRect) nv.Scissor(...this._lastRect);
    else reportError(new Error('ctx.clip(): only a single rect()/roundRect() path is supported by this emulation'));
  }

  resetClip() {
    nv.ResetScissor();
  }

  fillRect(x, y, w, h) {
    nv.BeginPath();
    nv.Rect(x, y, w, h);
    this._applyFillStyle();
    nv.Fill();
  }

  strokeRect(x, y, w, h) {
    nv.BeginPath();
    nv.Rect(x, y, w, h);
    this._applyStrokeStyle();
    nv.Stroke();
  }

  clearRect(x, y, w, h) {
    // No true alpha-clear is available (see file header); paint over with
    // the background color instead, which is correct for the common case of
    // a full-canvas clear at the top of a frame.
    nv.Save();
    nv.BeginPath();
    nv.Rect(x, y, w, h);
    nv.FillColor(parseColor(clearRectColor));
    nv.Fill();
    nv.Restore();
  }

  /* -- images -- */

  drawImage(image, ...args) {
    const iw = image.width;
    const ih = image.height;
    const id = image._id;
    if(id == null || id < 0) return;

    let sx = 0,
      sy = 0,
      sw = iw,
      sh = ih,
      dx,
      dy,
      dw,
      dh;

    if(args.length >= 8) [sx, sy, sw, sh, dx, dy, dw, dh] = args;
    else if(args.length >= 4) [dx, dy, dw, dh] = args;
    else [dx, dy] = args, (dw = iw), (dh = ih);

    const scaleX = dw / sw;
    const scaleY = dh / sh;
    const ox = dx - sx * scaleX;
    const oy = dy - sy * scaleY;

    nv.Save();
    nv.BeginPath();
    nv.Rect(dx, dy, dw, dh);
    const pattern = nv.ImagePattern(ox, oy, iw * scaleX, ih * scaleY, 0, id, this._state.globalAlpha);
    nv.FillPaint(pattern);
    nv.Fill();
    nv.Restore();
  }

  /* -- gradients / patterns -- */

  createLinearGradient(x0, y0, x1, y1) {
    return new CanvasGradient('linear', [x0, y0, x1, y1]);
  }

  createRadialGradient(x0, y0, r0, x1, y1, r1) {
    return new CanvasGradient('radial', [x0, y0, r0, x1, y1, r1]);
  }

  createPattern(image) {
    return new CanvasPattern(image);
  }

  /* -- text -- */

  fillText(text, x, y) {
    this._applyFillStyle();
    nv.Text(x, y, String(text));
  }

  strokeText(text, x, y) {
    // nanovg draws filled glyphs only; approximate a stroke with the stroke color.
    nv.FillColor(parseColor(this._state.strokeStyle));
    nv.Text(x, y, String(text));
  }

  measureText(text) {
    const bounds = {};
    const advance = nv.TextBounds(0, 0, String(text), null, bounds);
    return { width: (bounds.xmax ?? advance) - (bounds.xmin ?? 0), actualBoundingBoxLeft: 0, actualBoundingBoxRight: advance };
  }

  /* -- misc no-ops kept for API compatibility -- */

  setLineDash() {}
  getLineDash() {
    return [];
  }
}

let clearRectColor = '#000000';

/* -------------------------------------------------------- animation loop */

let rafCallbacks = new Map();
let rafId = 1;
let loopStarted = false;
let stopped = false;

function requestAnimationFrame(cb) {
  const id = rafId++;
  rafCallbacks.set(id, cb);
  if(!loopStarted) {
    loopStarted = true;
    os.setTimeout(tick, 0);
  }
  return id;
}

function cancelAnimationFrame(id) {
  rafCallbacks.delete(id);
}

function tick() {
  if(stopped) return;

  glfw.poll();

  if(glfwWindow.shouldClose) {
    stopped = true;
    window.dispatchEvent(makeEvent('close', window));
    return;
  }

  const due = rafCallbacks;
  rafCallbacks = new Map();

  nv.BeginFrame(canvasWidth, canvasHeight, 1);
  const ts = nowMs();
  for(const cb of due.values()) {
    try {
      cb(ts);
    } catch(e) {
      reportError(e);
    }
  }
  nv.EndFrame();
  glfwWindow.swapBuffers();

  os.setTimeout(tick, FRAME_MS);
}

/* ---------------------------------------------------------------- globals */

const Canvas2D = {
  window,
  document,
  canvas,
  nvg: nv,
  glfw,
  glfwWindow,
  registerFont,
  set clearColor(v) {
    clearRectColor = v;
  },
  get clearColor() {
    return clearRectColor;
  },
};

Object.assign(globalThis, {
  window,
  document,
  navigator: window.navigator,
  performance,
  requestAnimationFrame,
  cancelAnimationFrame,
  setTimeout,
  clearTimeout,
  setInterval,
  clearInterval,
  Image,
  CanvasRenderingContext2D,
  CanvasGradient,
  CanvasPattern,
});

export { window, document, canvas, Image, CanvasRenderingContext2D, CanvasGradient, CanvasPattern, requestAnimationFrame, cancelAnimationFrame, registerFont, Canvas2D };
export default Canvas2D;
