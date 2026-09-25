import { ALIGN_CENTER, ALIGN_LEFT, ALIGN_MIDDLE, ALIGN_RIGHT, ANTIALIAS, CreateGL3, RGB } from './nanovg.js';

const HANDLE_R = 6;
const HIT_R = 10;
const SAVE_PATH = 'polylines.json';

const COL = {
  bg: RGB(24, 28, 36),
  line: RGB(120, 200, 255),
  handle: RGB(40, 90, 160),
  hover: RGB(90, 150, 230),
  drag: RGB(245, 200, 70),
  outline: RGB(235, 240, 248),
  text: RGB(170, 180, 195),
  btn: RGB(50, 90, 160),
  btnHot: RGB(70, 120, 200),
  btnText: RGB(235, 240, 248),
  ok: RGB(120, 220, 140),
};

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
  // Fallback web-safe or typical font loading paths for NanoVG
  for(let path of ['DejaVuSans.ttf', './DejaVuSans.ttf', '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']) {
    if((font = nvg.CreateFont('sans', path)) >= 0) break;
  }
  const hasText = font >= 0;

  let polylines = [
    [
      { x: 180, y: 200 },
      { x: 360, y: 140 },
      { x: 520, y: 260 },
      { x: 700, y: 180 },
    ],
    [
      { x: 240, y: 520 },
      { x: 430, y: 460 },
      { x: 600, y: 560 },
      { x: 800, y: 480 },
    ],
  ];

  let mouse = { x: 0, y: 0 };
  let drag = null;
  let active = 0;
  let savedAt = 0;

  const inRect = (p, r) => p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;

  function hitHandle(mx, my) {
    let best = null;
    let bestD2 = HIT_R * HIT_R;
    for(let pl = 0; pl < polylines.length; pl++) {
      const pts = polylines[pl];
      for(let pt = 0; pt < pts.length; pt++) {
        const dx = pts[pt].x - mx;
        const dy = pts[pt].y - my;
        const d2 = dx * dx + dy * dy;
        if(d2 <= bestD2) {
          bestD2 = d2;
          best = { pl, pt };
        }
      }
    }
    return best;
  }

  function save() {
    const dataStr = 'data:text/json;charset=utf-8,' + encodeURIComponent(JSON.stringify(polylines, null, 2));
    const downloadAnchor = document.createElement('a');
    downloadAnchor.setAttribute('href', dataStr);
    downloadAnchor.setAttribute('download', SAVE_PATH);
    document.body.appendChild(downloadAnchor);
    downloadAnchor.click();
    downloadAnchor.remove();

    savedAt = Date.now();
    console.log('saved points to browser download');
  }

  // Event Listeners
  window.addEventListener('mousemove', e => {
    const rect = canvas.getBoundingClientRect();
    mouse.x = e.clientX - rect.left;
    mouse.y = e.clientY - rect.top;
    if(drag) polylines[drag.pl][drag.pt] = { x: mouse.x, y: mouse.y };
  });

  window.addEventListener('mousedown', e => {
    const width = window.innerWidth;
    const saveBtn = { x: width - 116, y: 16, w: 100, h: 34 };

    if(e.button === 0) {
      if(inRect(mouse, saveBtn)) {
        save();
        return;
      }
      const hit = hitHandle(mouse.x, mouse.y);
      if(hit) {
        drag = hit;
        active = hit.pl;
      } else {
        polylines[active].push({ x: mouse.x, y: mouse.y });
      }
    } else if(e.button === 2) {
      e.preventDefault();
      const hit = hitHandle(mouse.x, mouse.y);
      if(hit && polylines[hit.pl].length > 2) polylines[hit.pl].splice(hit.pt, 1);
    }
  });

  window.addEventListener('mouseup', e => {
    if(e.button === 0) drag = null;
  });

  window.addEventListener('contextmenu', e => e.preventDefault());

  window.addEventListener('keydown', e => {
    if(e.key === 's' || e.key === 'S') save();
  });

  function text(x, y, str, { size = 14, color = COL.text, align = ALIGN_LEFT | ALIGN_MIDDLE } = {}) {
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

    const saveBtn = { x: width - 116, y: 16, w: 100, h: 34 };

    nvg.BeginFrame(width, height, window.devicePixelRatio || 1);

    nvg.BeginPath();
    nvg.Rect(0, 0, width, height);
    nvg.FillColor(COL.bg);
    nvg.Fill();

    const hover = drag || hitHandle(mouse.x, mouse.y);

    for(const pts of polylines) {
      if(pts.length < 2) continue;
      nvg.BeginPath();
      nvg.MoveTo(pts[0].x, pts[0].y);
      for(let i = 1; i < pts.length; i++) nvg.LineTo(pts[i].x, pts[i].y);
      nvg.StrokeColor(COL.line);
      nvg.StrokeWidth(2);
      nvg.Stroke();
    }

    for(let pl = 0; pl < polylines.length; pl++) {
      const pts = polylines[pl];
      for(let pt = 0; pt < pts.length; pt++) {
        const isDrag = drag && drag.pl === pl && drag.pt === pt;
        const isHover = hover && hover.pl === pl && hover.pt === pt;
        nvg.BeginPath();
        nvg.Circle(pts[pt].x, pts[pt].y, HANDLE_R);
        nvg.FillColor(isDrag ? COL.drag : isHover ? COL.hover : COL.handle);
        nvg.Fill();
        nvg.StrokeColor(COL.outline);
        nvg.StrokeWidth(1.5);
        nvg.Stroke();
      }
    }

    const hot = inRect(mouse, saveBtn);
    nvg.BeginPath();
    nvg.RoundedRect(saveBtn.x, saveBtn.y, saveBtn.w, saveBtn.h, 6);
    nvg.FillColor(hot ? COL.btnHot : COL.btn);
    nvg.Fill();
    text(saveBtn.x + saveBtn.w / 2, saveBtn.y + saveBtn.h / 2, 'Save (s)', {
      color: COL.btnText,
      align: ALIGN_CENTER | ALIGN_MIDDLE,
    });

    if(Date.now() - savedAt < 1200) text(saveBtn.x - 12, saveBtn.y + saveBtn.h / 2, 'saved ✓', { color: COL.ok, align: ALIGN_RIGHT | ALIGN_MIDDLE });

    text(16, height - 18, 'drag handle: move    left-click: add point    right-click: delete    s: save', { size: 12 });

    nvg.EndFrame();
    requestAnimationFrame(render);
  }

  requestAnimationFrame(render);
}

main();
