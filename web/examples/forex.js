import { ALIGN_BASELINE, ALIGN_LEFT, ALIGN_MIDDLE, ALIGN_RIGHT, ALIGN_TOP, ANTIALIAS, CreateGL3, RGB, RGBA } from '../nanovg.js';

const SYMBOL = 'EUR/USD';
const CANDLE_MS = 1000;
const MAX_CANDLES = 80;
const TICK_VOL = 0.0006;

const M = { top: 64, right: 84, bottom: 28, left: 12 };

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
  const canvas =
    document.getElementById('canvas') ||
    document.querySelector('canvas') ||
    (() => {
      const c = document.createElement('canvas');
      c.id = 'canvas';
      document.body.appendChild(c);
      return c;
    })();

  const nvg = CreateGL3(ANTIALIAS);

  let font = -1;
  for(let path of ['DejaVuSans.ttf', './DejaVuSans.ttf', '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']) {
    if((font = nvg.CreateFont('sans', path)) >= 0) break;
  }
  const hasText = font >= 0;

  let price = 1.1;
  const candles = [];

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

  window.addEventListener('keydown', e => {
    if(e.code === 'Space') {
      paused = !paused;
      e.preventDefault();
    }
  });

  function text(x, y, str, { size = 13, color = COL.text, align = ALIGN_LEFT | ALIGN_BASELINE } = {}) {
    if(!hasText) return;
    nvg.FontFace('sans');
    nvg.FontSize(size);
    nvg.FillColor(color);
    nvg.TextAlign(align);
    nvg.Text(x, y, str);
  }

  function render() {
    const width = window.innerWidth;
    const height = window.innerHeight;

    if(canvas.width !== width || canvas.height !== height) {
      canvas.width = width;
      canvas.height = height;
    }

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

    nvg.BeginFrame(width, height, window.devicePixelRatio || 1);

    nvg.BeginPath();
    nvg.Rect(0, 0, width, height);
    nvg.FillColor(COL.bg);
    nvg.Fill();

    const plotW = width - M.left - M.right;
    const plotH = height - M.top - M.bottom;
    const slotW = plotW / MAX_CANDLES;
    const bodyW = Math.max(2, slotW * 0.6);

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

    for(let i = 0; i < candles.length; i++) {
      const c = candles[i];
      const x = xOf(i);
      const bull = c.close >= c.open;
      const color = bull ? COL.bull : COL.bear;

      nvg.BeginPath();
      nvg.MoveTo(x, yOf(c.high));
      nvg.LineTo(x, yOf(c.low));
      nvg.StrokeColor(color);
      nvg.StrokeWidth(1);
      nvg.Stroke();

      const yo = yOf(c.open);
      const yc = yOf(c.close);
      const top = Math.min(yo, yc);
      const h = Math.max(1, Math.abs(yc - yo));
      nvg.BeginPath();
      nvg.Rect(x - bodyW / 2, top, bodyW, h);
      nvg.FillColor(color);
      nvg.Fill();
    }

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

    const dir = live.close >= live.open ? COL.bull : COL.bear;
    text(M.left + 4, 30, SYMBOL, { size: 22, color: COL.title });
    text(M.left + 150, 30, price.toFixed(5), { size: 22, color: dir });
    text(M.left + 4, 50, `O ${live.open.toFixed(5)}   H ${live.high.toFixed(5)}   L ${live.low.toFixed(5)}   C ${live.close.toFixed(5)}`, { size: 12 });
    text(width - M.right, 50, paused ? 'PAUSED' : 'LIVE', {
      color: paused ? COL.last : COL.bull,
      align: ALIGN_RIGHT | ALIGN_TOP,
    });
    text(M.left + 4, height - 8, 'space: pause/resume', { size: 11 });

    nvg.EndFrame();
    requestAnimationFrame(render);
  }

  requestAnimationFrame(render);
}

main();
