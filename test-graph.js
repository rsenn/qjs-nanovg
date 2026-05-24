// Realtime Forex candlestick chart.
//
// There is no live data feed here, so a streaming price is simulated with a
// random walk; ticks are aggregated into OHLC candles (one per `CANDLE_MS`),
// exactly as a real feed would be. Run with: qjsm test-graph.js
//
// Press 's' to save the current frame to a PPM image (forex-<timestamp>.ppm).

import * as glfw from 'glfw';
import * as std from 'std';
import { ALIGN_BOTTOM, ALIGN_LEFT, ALIGN_MIDDLE, ALIGN_RIGHT, ALIGN_TOP, ANTIALIAS, CreateGL3, DeleteGL3, ReadPixels, RGB, RGBA, STENCIL_STROKES } from 'nanovg';

const SYMBOL = 'EUR/USD';
const CANDLE_MS = 1000; // duration of one candle
const MAX_CANDLES = 80; // visible history
const TICK_VOL = 0.0006; // per-tick price step magnitude

// Layout margins (px) around the plot area.
const M = { top: 64, right: 84, bottom: 28, left: 12 };

// Palette
const COL = {
  bg: RGB(18, 22, 30),
  grid: RGBA(255, 255, 255, 18),
  axis: RGBA(255, 255, 255, 40),
  text: RGB(150, 162, 176),
  title: RGB(225, 230, 238),
  bull: RGB(38, 166, 154),
  bear: RGB(239, 83, 80),
  last: RGB(245, 200, 70),
};

function newCandle(open) {
  return { open, high: open, low: open, close: open };
}

function tick(candle, price) {
  candle.close = price;
  if(price > candle.high) candle.high = price;
  if(price < candle.low) candle.low = price;
}

function main() {
  glfw.Window.hint(glfw.CONTEXT_VERSION_MAJOR, 3);
  glfw.Window.hint(glfw.CONTEXT_VERSION_MINOR, 2);
  glfw.Window.hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE);
  glfw.Window.hint(glfw.OPENGL_FORWARD_COMPAT, true);
  glfw.Window.hint(glfw.RESIZABLE, false);
  glfw.Window.hint(glfw.SAMPLES, 4);

  const window = (glfw.context.current = new glfw.Window(1024, 640, `Forex — ${SYMBOL}`));
  const { width, height } = window.size;

  const nvg = CreateGL3(STENCIL_STROKES | ANTIALIAS);

  // Load a font for the axis/HUD labels; degrade gracefully if unavailable.
  let font = -1;
  for(let path of ['/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', '/usr/share/fonts/TTF/DejaVuSans.ttf', '/usr/share/fonts/dejavu/DejaVuSans.ttf']) {
    if((font = nvg.CreateFont('sans', path)) >= 0) break;
  }
  const hasText = font >= 0;

  // ---- Simulated price feed -------------------------------------------------
  let price = 1.1;
  const candles = [];

  // Seed some history so the chart starts populated.
  for(let k = 0; k < MAX_CANDLES * 0.5; k++) {
    const c = newCandle(price);
    for(let t = 0; t < 20; t++) {
      price += (Math.random() - 0.5) * TICK_VOL;
      tick(c, price);
    }
    candles.push(c);
  }

  let live = newCandle(price);
  candles.push(live);
  let candleStart = Date.now();

  let paused = false;
  let wantShot = false; // set on 's', consumed in the render loop

  Object.assign(window, {
    handleKey(keyCode, scancode, action, mods) {
      if(!action) return;
      if(keyCode === glfw.KEY_ESCAPE || keyCode === glfw.KEY_Q) running = false;
      if(keyCode === glfw.KEY_S) wantShot = true;
    },
    handleCharMods(charCode) {
      if(String.fromCodePoint(charCode) === ' ') paused = !paused;
    },
  });

  let running = true;

  function text(x, y, str, { size = 13, color = COL.text, align = ALIGN_LEFT | ALIGN_BASELINE } = {}) {
    if(!hasText) return;
    nvg.FontFace('sans');
    nvg.FontSize(size);
    nvg.FillColor(color);
    nvg.TextAlign(align);
    nvg.Text(x, y, str);
  }

  // Grab the rendered frame and write it as a binary PPM (P6). ReadPixels
  // returns RGBA bytes with a bottom-left origin, so rows are flipped here.
  function saveScreenshot() {
    const rgba = new Uint8Array(ReadPixels(width, height));
    const body = new Uint8Array(width * height * 3);

    for(let y = 0; y < height; y++) {
      const srcRow = (height - 1 - y) * width;
      const dstRow = y * width;
      for(let x = 0; x < width; x++) {
        const s = (srcRow + x) * 4;
        const d = (dstRow + x) * 3;
        body[d] = rgba[s];
        body[d + 1] = rgba[s + 1];
        body[d + 2] = rgba[s + 2];
      }
    }

    const name = `forex-${Date.now()}.ppm`;
    const f = std.open(name, 'wb');
    if(!f) {
      console.log('screenshot: could not open', name);
      return;
    }
    f.puts(`P6\n${width} ${height}\n255\n`);
    f.write(body.buffer, 0, body.byteLength);
    f.close();
    console.log('screenshot saved:', name);
  }

  function draw() {
    // Clear by painting the background (no glClear binding is exposed).
    nvg.BeginPath();
    nvg.Rect(0, 0, width, height);
    nvg.FillColor(COL.bg);
    nvg.Fill();

    const plotW = width - M.left - M.right;
    const plotH = height - M.top - M.bottom;
    const slotW = plotW / MAX_CANDLES;
    const bodyW = Math.max(2, slotW * 0.6);

    // Price range over the visible candles, with padding.
    let lo = Infinity,
      hi = -Infinity;
    for(let c of candles) {
      if(c.low < lo) lo = c.low;
      if(c.high > hi) hi = c.high;
    }
    const pad = (hi - lo) * 0.08 || 0.0005;
    lo -= pad;
    hi += pad;

    const yOf = p => M.top + (1 - (p - lo) / (hi - lo)) * plotH;
    const xOf = i => M.left + (i + 0.5) * slotW;

    // Horizontal grid + price scale (right axis).
    const divs = 5;
    for(let k = 0; k <= divs; k++) {
      const p = lo + ((hi - lo) * k) / divs;
      const y = yOf(p);
      nvg.BeginPath();
      nvg.MoveTo(M.left, y);
      nvg.LineTo(M.left + plotW, y);
      nvg.StrokeColor(COL.grid);
      nvg.StrokeWidth(1);
      nvg.Stroke();
      text(M.left + plotW + 6, y, p.toFixed(5), { align: ALIGN_LEFT | ALIGN_MIDDLE });
    }

    // Candles.
    for(let i = 0; i < candles.length; i++) {
      const c = candles[i];
      const x = xOf(i);
      const bull = c.close >= c.open;
      const color = bull ? COL.bull : COL.bear;

      // Wick (high -> low).
      nvg.BeginPath();
      nvg.MoveTo(x, yOf(c.high));
      nvg.LineTo(x, yOf(c.low));
      nvg.StrokeColor(color);
      nvg.StrokeWidth(1);
      nvg.Stroke();

      // Body (open <-> close).
      const yo = yOf(c.open);
      const yc = yOf(c.close);
      const top = Math.min(yo, yc);
      const h = Math.max(1, Math.abs(yc - yo));
      nvg.BeginPath();
      nvg.Rect(x - bodyW / 2, top, bodyW, h);
      nvg.FillColor(color);
      nvg.Fill();
    }

    // Last-price marker line + label.
    const lastY = yOf(price);
    nvg.BeginPath();
    nvg.MoveTo(M.left, lastY);
    nvg.LineTo(M.left + plotW, lastY);
    nvg.StrokeColor(COL.last);
    nvg.StrokeWidth(1);
    nvg.Stroke();

    nvg.BeginPath();
    nvg.Rect(M.left + plotW, lastY - 9, M.right, 18);
    nvg.FillColor(COL.last);
    nvg.Fill();
    text(M.left + plotW + 6, lastY, price.toFixed(5), {
      color: RGB(20, 20, 20),
      align: ALIGN_LEFT | ALIGN_MIDDLE,
    });

    // HUD: symbol + current price + OHLC of the live candle.
    const dir = live.close >= live.open ? COL.bull : COL.bear;
    text(M.left + 4, 30, SYMBOL, { size: 22, color: COL.title });
    text(M.left + 150, 30, price.toFixed(5), { size: 22, color: dir });
    text(M.left + 4, 50, `O ${live.open.toFixed(5)}   H ${live.high.toFixed(5)}   L ${live.low.toFixed(5)}   C ${live.close.toFixed(5)}`, { size: 12 });
    text(width - M.right, 50, paused ? 'PAUSED' : 'LIVE', {
      color: paused ? COL.last : COL.bull,
      align: ALIGN_RIGHT | ALIGN_TOP,
    });
    text(M.left + 4, height - 8, 'space: pause/resume    s: screenshot    q/esc: quit', { size: 11 });
  }

  while((running &&= !window.shouldClose)) {
    if(!paused) {
      price += (Math.random() - 0.5) * TICK_VOL;
      tick(live, price);

      if(Date.now() - candleStart >= CANDLE_MS) {
        candleStart = Date.now();
        live = newCandle(price);
        candles.push(live);
        if(candles.length > MAX_CANDLES) candles.shift();
      }
    }

    nvg.BeginFrame(width, height, 1);
    draw();
    nvg.EndFrame();

    if(wantShot) {
      saveScreenshot();
      wantShot = false;
    }

    window.swapBuffers();
    glfw.poll();
  }

  DeleteGL3(nvg);
  window.destroy();
}

main();
