// Interactive polyline / bezier curve editor.
//
// Shows a set of paths whose vertices are draggable handles. Each path has a
// type — straight line, quadratic bezier, or cubic bezier — chosen with the
// on-screen toolbar buttons (the buttons retype the *active* path, the one you
// last touched). Quad/cubic paths also expose draggable control-point handles
// (small squares) joined to their anchors by a faint control polygon.
//
// Drag a handle to move it, left-click empty canvas to append a point to the
// active path, right-click an anchor to delete it. Save to `polylines.json`
// with the 's' key or the on-screen Save button.
//
// Run: qjsm curve-editor.js

import * as glfw from 'glfw';
import * as std from 'std';
import { ALIGN_CENTER, ALIGN_LEFT, ALIGN_MIDDLE, ALIGN_RIGHT, ANTIALIAS, CreateGL3, DeleteGL3, RGB, STENCIL_STROKES } from 'nanovg';

// glfw.so does not export the mouse-button / action enums, so spell them out.
// (GLFW_MOUSE_BUTTON_LEFT = 0, GLFW_MOUSE_BUTTON_RIGHT = 1; GLFW_PRESS = 1.)
const MB_LEFT = 0;
const MB_RIGHT = 1;
const PRESS = 1;

const HANDLE_R = 6; // drawn anchor-handle radius
const HIT_R = 10; // click tolerance around a handle
const SAVE_PATH = 'polylines.json';

const TYPES = ['line', 'quad', 'cubic'];
const LABELS = { line: 'Line', quad: 'Quad', cubic: 'Cubic' };

const COL = {
  bg: RGB(24, 28, 36),
  line: RGB(90, 140, 180), // inactive path
  lineActive: RGB(120, 200, 255), // active path
  handle: RGB(40, 90, 160), // anchor
  ctrl: RGB(200, 140, 70), // bezier control point
  ctrlLine: RGB(120, 125, 135), // control polygon
  hover: RGB(90, 150, 230),
  drag: RGB(245, 200, 70),
  outline: RGB(235, 240, 248),
  text: RGB(170, 180, 195),
  btn: RGB(50, 90, 160),
  btnHot: RGB(70, 120, 200),
  btnSel: RGB(90, 160, 235),
  btnText: RGB(235, 240, 248),
  ok: RGB(120, 220, 140),
};

// Control points for the segment ending at pts[i] depend on the path type and
// are recomputed (placed along the segment) whenever the type changes.
function initSegment(p, i) {
  const a = p.pts[i - 1];
  const b = p.pts[i];
  if(p.type === 'quad') {
    b.c = { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 };
  } else if(p.type === 'cubic') {
    b.c1 = { x: a.x + (b.x - a.x) / 3, y: a.y + (b.y - a.y) / 3 };
    b.c2 = { x: a.x + ((b.x - a.x) * 2) / 3, y: a.y + ((b.y - a.y) * 2) / 3 };
  }
}

function setType(p, type) {
  p.type = type;
  for(let i = 1; i < p.pts.length; i++) initSegment(p, i);
}

function main() {
  glfw.Window.hint(glfw.CONTEXT_VERSION_MAJOR, 3);
  glfw.Window.hint(glfw.CONTEXT_VERSION_MINOR, 2);
  glfw.Window.hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE);
  glfw.Window.hint(glfw.OPENGL_FORWARD_COMPAT, true);
  glfw.Window.hint(glfw.RESIZABLE, false);
  glfw.Window.hint(glfw.SAMPLES, 4);

  const window = (glfw.context.current = new glfw.Window(1024, 768, 'Curve editor'));
  const { width, height } = window.size;

  const nvg = CreateGL3(STENCIL_STROKES | ANTIALIAS);

  let font = -1;
  for(let path of ['/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', '/usr/share/fonts/TTF/DejaVuSans.ttf', '/usr/share/fonts/dejavu/DejaVuSans.ttf']) {
    if((font = nvg.CreateFont('sans', path)) >= 0) break;
  }
  const hasText = font >= 0;

  // Two seed paths: a straight polyline and a cubic bezier to show the feature.
  let polylines = [
    {
      type: 'line',
      pts: [
        { x: 180, y: 200 },
        { x: 360, y: 140 },
        { x: 520, y: 260 },
        { x: 700, y: 180 },
      ],
    },
    {
      type: 'cubic',
      pts: [
        { x: 240, y: 520 },
        { x: 430, y: 460 },
        { x: 600, y: 560 },
        { x: 800, y: 480 },
      ],
    },
  ];
  for(const p of polylines) setType(p, p.type); // seed control points

  const saveBtn = { x: width - 116, y: 16, w: 100, h: 34 };
  const typeBtns = TYPES.map((type, i) => ({ type, x: 16 + i * 84, y: 16, w: 76, h: 34 }));

  let mouse = { x: width / 2, y: height / 2 };
  let drag = null; // { pl, pt, ctrl } currently being dragged
  let active = 0; // path that left-clicks append to / buttons retype
  let savedAt = 0; // timestamp of last save, for on-screen feedback
  let running = true;

  const inRect = (p, r) => p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;

  // Coordinate object a handle reference points at (anchor or control point).
  function ptOf(h) {
    const pt = polylines[h.pl].pts[h.pt];
    return h.ctrl ? pt[h.ctrl] : pt;
  }

  // Visit every handle. ctrl is null for anchors, or 'c' / 'c1' / 'c2' for the
  // control points of the segment ending at pts[pt]. Control points only exist
  // on curved paths and never on the first point (it has no incoming segment).
  function eachHandle(fn, anchorsOnly) {
    for(let pl = 0; pl < polylines.length; pl++) {
      const p = polylines[pl];
      for(let pt = 0; pt < p.pts.length; pt++) {
        fn(pl, pt, null);
        if(anchorsOnly || pt === 0) continue;
        if(p.type === 'quad') fn(pl, pt, 'c');
        else if(p.type === 'cubic') {
          fn(pl, pt, 'c1');
          fn(pl, pt, 'c2');
        }
      }
    }
  }

  // Nearest handle within HIT_R of (mx, my), or null.
  function hitHandle(mx, my, anchorsOnly) {
    let best = null;
    let bestD2 = HIT_R * HIT_R;
    eachHandle((pl, pt, ctrl) => {
      const o = ctrl ? polylines[pl].pts[pt][ctrl] : polylines[pl].pts[pt];
      const dx = o.x - mx;
      const dy = o.y - my;
      const d2 = dx * dx + dy * dy;
      if(d2 <= bestD2) {
        bestD2 = d2;
        best = { pl, pt, ctrl };
      }
    }, anchorsOnly);
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
    console.log(
      'saved',
      polylines.reduce((n, p) => n + p.pts.length, 0),
      'points to',
      SAVE_PATH,
    );
  }

  Object.assign(window, {
    handleCursorPos(x, y) {
      mouse = { x, y };
      if(drag) {
        const o = ptOf(drag);
        o.x = x;
        o.y = y;
      }
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
        for(const b of typeBtns) {
          if(inRect(mouse, b)) {
            setType(polylines[active], b.type);
            return;
          }
        }
        const hit = hitHandle(mouse.x, mouse.y);
        if(hit) {
          drag = hit;
          active = hit.pl;
        } else {
          const p = polylines[active];
          p.pts.push({ x: mouse.x, y: mouse.y });
          if(p.pts.length >= 2) initSegment(p, p.pts.length - 1);
        }
      } else if(button === MB_RIGHT) {
        const hit = hitHandle(mouse.x, mouse.y, true); // anchors only
        if(hit && polylines[hit.pl].pts.length > 2) polylines[hit.pl].pts.splice(hit.pt, 1);
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

  // Trace a path's outline (line / quad / cubic) into the current NanoVG path.
  function tracePath(p) {
    const pts = p.pts;
    nvg.MoveTo(pts[0].x, pts[0].y);
    for(let i = 1; i < pts.length; i++) {
      const b = pts[i];
      if(p.type === 'quad') nvg.QuadTo(b.c.x, b.c.y, b.x, b.y);
      else if(p.type === 'cubic') nvg.BezierTo(b.c1.x, b.c1.y, b.c2.x, b.c2.y, b.x, b.y);
      else nvg.LineTo(b.x, b.y);
    }
  }

  function draw() {
    nvg.BeginPath();
    nvg.Rect(0, 0, width, height);
    nvg.FillColor(COL.bg);
    nvg.Fill();

    const hover = drag || hitHandle(mouse.x, mouse.y);

    // Path outlines.
    for(let pl = 0; pl < polylines.length; pl++) {
      const p = polylines[pl];
      if(p.pts.length < 2) continue;
      nvg.BeginPath();
      tracePath(p);
      nvg.StrokeColor(pl === active ? COL.lineActive : COL.line);
      nvg.StrokeWidth(2);
      nvg.Stroke();
    }

    // Control polygons, drawn under the handles.
    for(const p of polylines) {
      if(p.type === 'line') continue;
      nvg.BeginPath();
      for(let i = 1; i < p.pts.length; i++) {
        const a = p.pts[i - 1];
        const b = p.pts[i];
        if(p.type === 'quad') {
          nvg.MoveTo(a.x, a.y);
          nvg.LineTo(b.c.x, b.c.y);
          nvg.LineTo(b.x, b.y);
        } else {
          nvg.MoveTo(a.x, a.y);
          nvg.LineTo(b.c1.x, b.c1.y);
          nvg.MoveTo(b.x, b.y);
          nvg.LineTo(b.c2.x, b.c2.y);
        }
      }
      nvg.StrokeColor(COL.ctrlLine);
      nvg.StrokeWidth(1);
      nvg.Stroke();
    }

    // Handles: anchors as circles, control points as small squares.
    eachHandle((pl, pt, ctrl) => {
      const o = ctrl ? polylines[pl].pts[pt][ctrl] : polylines[pl].pts[pt];
      const isDrag = drag && drag.pl === pl && drag.pt === pt && drag.ctrl === ctrl;
      const isHover = hover && hover.pl === pl && hover.pt === pt && hover.ctrl === ctrl;
      const r = ctrl ? HANDLE_R - 1 : HANDLE_R;
      nvg.BeginPath();
      if(ctrl) nvg.Rect(o.x - r, o.y - r, r * 2, r * 2);
      else nvg.Circle(o.x, o.y, r);
      nvg.FillColor(isDrag ? COL.drag : isHover ? COL.hover : ctrl ? COL.ctrl : COL.handle);
      nvg.Fill();
      nvg.StrokeColor(COL.outline);
      nvg.StrokeWidth(1.5);
      nvg.Stroke();
    });

    // Toolbar: type buttons (left), Save (right).
    for(const b of typeBtns) {
      const sel = polylines[active].type === b.type;
      const hot = inRect(mouse, b);
      nvg.BeginPath();
      nvg.RoundedRect(b.x, b.y, b.w, b.h, 6);
      nvg.FillColor(sel ? COL.btnSel : hot ? COL.btnHot : COL.btn);
      nvg.Fill();
      text(b.x + b.w / 2, b.y + b.h / 2, LABELS[b.type], { color: COL.btnText, align: ALIGN_CENTER | ALIGN_MIDDLE });
    }
    text(typeBtns[2].x + typeBtns[2].w + 16, typeBtns[0].y + typeBtns[0].h / 2, 'active: path ' + (active + 1), { size: 12 });

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

    text(16, height - 18, 'drag handle: move    left-click: add point    right-click: delete    Line/Quad/Cubic: retype active path    s: save    q/esc: quit', { size: 12 });
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
