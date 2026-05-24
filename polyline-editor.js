// Interactive polyline editor.
//
// Shows a set of polylines whose vertices are draggable handles. Drag a handle
// to move a point, left-click empty canvas to append a point to the active
// polyline, right-click a handle to delete it. Save the result to
// `polylines.json` with the 's' key or the on-screen Save button.
//
// Run: qjsm polyline-editor.js

import * as glfw from 'glfw';
import * as std from 'std';
import { ALIGN_CENTER, ALIGN_LEFT, ALIGN_MIDDLE, ANTIALIAS, CreateGL3, DeleteGL3, RGB, RGBA, STENCIL_STROKES } from 'nanovg';

// glfw.so does not export the mouse-button / action enums, so spell them out.
// (GLFW_MOUSE_BUTTON_LEFT = 0, GLFW_MOUSE_BUTTON_RIGHT = 1; GLFW_PRESS = 1.)
const MB_LEFT = 0;
const MB_RIGHT = 1;
const PRESS = 1;

const HANDLE_R = 6; // drawn handle radius
const HIT_R = 10; // click tolerance around a handle
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
  glfw.Window.hint(glfw.CONTEXT_VERSION_MAJOR, 3);
  glfw.Window.hint(glfw.CONTEXT_VERSION_MINOR, 2);
  glfw.Window.hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE);
  glfw.Window.hint(glfw.OPENGL_FORWARD_COMPAT, true);
  glfw.Window.hint(glfw.RESIZABLE, false);
  glfw.Window.hint(glfw.SAMPLES, 4);

  const window = (glfw.context.current = new glfw.Window(1024, 768, 'Polyline editor'));
  const { width, height } = window.size;

  const nvg = CreateGL3(STENCIL_STROKES | ANTIALIAS);

  let font = -1;
  for(let path of [
    '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
    '/usr/share/fonts/TTF/DejaVuSans.ttf',
    '/usr/share/fonts/dejavu/DejaVuSans.ttf',
  ]) {
    if((font = nvg.CreateFont('sans', path)) >= 0) break;
  }
  const hasText = font >= 0;

  // Two seed polylines. Each point is an editable handle.
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

  const saveBtn = { x: width - 116, y: 16, w: 100, h: 34 };

  let mouse = { x: width / 2, y: height / 2 };
  let drag = null; // { pl, pt } currently being dragged
  let active = 0; // polyline that left-clicks append to
  let savedAt = 0; // timestamp of last save, for on-screen feedback
  let running = true;

  const inRect = (p, r) => p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;

  // Nearest handle within HIT_R of (mx, my), or null.
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
    const f = std.open(SAVE_PATH, 'w');
    if(!f) {
      console.log('save: could not open', SAVE_PATH);
      return;
    }
    f.puts(JSON.stringify(polylines, null, 2));
    f.close();
    savedAt = Date.now();
    console.log('saved', polylines.reduce((n, p) => n + p.length, 0), 'points to', SAVE_PATH);
  }

  Object.assign(window, {
    handleCursorPos(x, y) {
      mouse = { x, y };
      if(drag) polylines[drag.pl][drag.pt] = { x, y };
    },
    handleMouseButton(button, action) {
      if(action !== PRESS) {
        if(button === MB_LEFT) drag = null;
        return;
      }
      if(button === MB_LEFT) {
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
      } else if(button === MB_RIGHT) {
        const hit = hitHandle(mouse.x, mouse.y);
        if(hit && polylines[hit.pl].length > 2) polylines[hit.pl].splice(hit.pt, 1);
      }
    },
    handleKey(keyCode, scancode, action) {
      if(!action) return;
      if(keyCode === glfw.KEY_ESCAPE || keyCode === glfw.KEY_Q) running = false;
      if(keyCode === glfw.KEY_S) save();
    },
  });

  function text(x, y, str, { size = 14, color = COL.text, align = ALIGN_LEFT | ALIGN_MIDDLE } = {}) {
    if(!hasText) return;
    nvg.FontFace('sans');
    nvg.FontSize(size);
    nvg.FillColor(color);
    nvg.TextAlign(align);
    nvg.Text(x, y, str);
  }

  function draw() {
    nvg.BeginPath();
    nvg.Rect(0, 0, width, height);
    nvg.FillColor(COL.bg);
    nvg.Fill();

    const hover = drag || hitHandle(mouse.x, mouse.y);

    // Polyline segments.
    for(const pts of polylines) {
      if(pts.length < 2) continue;
      nvg.BeginPath();
      nvg.MoveTo(pts[0].x, pts[0].y);
      for(let i = 1; i < pts.length; i++) nvg.LineTo(pts[i].x, pts[i].y);
      nvg.StrokeColor(COL.line);
      nvg.StrokeWidth(2);
      nvg.Stroke();
    }

    // Handles, drawn on top of the lines.
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

    // Save button.
    const hot = inRect(mouse, saveBtn);
    nvg.BeginPath();
    nvg.RoundedRect(saveBtn.x, saveBtn.y, saveBtn.w, saveBtn.h, 6);
    nvg.FillColor(hot ? COL.btnHot : COL.btn);
    nvg.Fill();
    text(saveBtn.x + saveBtn.w / 2, saveBtn.y + saveBtn.h / 2, 'Save (s)', {
      color: COL.btnText,
      align: ALIGN_CENTER | ALIGN_MIDDLE,
    });

    if(Date.now() - savedAt < 1200)
      text(saveBtn.x - 12, saveBtn.y + saveBtn.h / 2, 'saved ✓', { color: COL.ok, align: ALIGN_RIGHT | ALIGN_MIDDLE });

    text(16, height - 18, 'drag handle: move    left-click: add point    right-click: delete    s: save    q/esc: quit', { size: 12 });
  }

  while((running &&= !window.shouldClose)) {
    nvg.BeginFrame(width, height, 1);
    draw();
    nvg.EndFrame();

    window.swapBuffers();
    glfw.poll();
  }

  DeleteGL3(nvg);
  window.destroy();
}

main();
