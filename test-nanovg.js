import * as glfw from 'glfw';
import { CreateGL3, STENCIL_STROKES, ANTIALIAS, DEBUG } from 'nanovg';

let window, nvg;

export function DrawImage(image, pos) {
  const size = nvg.ImageSize(image);
  nvg.Save();
  if(pos) nvg.Translate(...pos);
  nvg.BeginPath();
  nvg.Rect(0, 0, ...size);
  nvg.FillPaint(nvg.ImagePattern(0, 0, ...size, 0, image, 1));
  nvg.Fill();
  nvg.Restore();
}

export function DrawCircle(radius, stroke = nvg.RGB(255, 255, 255), fill = nvg.RGBA(255, 0, 0, 96)) {
  nvg.BeginPath();
  nvg.StrokeColor(stroke);
  nvg.StrokeWidth(3);
  if(fill) nvg.FillColor(fill);
  nvg.Circle(0, 0, radius);
  if(fill) nvg.Fill();
  nvg.Stroke();
}

function RotatePoint(x, y, angle) {
  let c = Math.cos(angle),
    s = Math.sin(angle);
  return [x * c - y * s, x * s + y * c];
}

export function Clear(color = nvg.RGB(0, 0, 0)) {
  const { size } = window;
  //console.log('size', ...size);
  nvg.Save();
  nvg.BeginPath();
  nvg.Rect(0, 0, ...size);
  nvg.FillColor(color);
  nvg.Fill();
  nvg.Restore();
}

function main(...args) {
  let i = 0;
  let running = true;

  glfw.Window.hint(glfw.CONTEXT_VERSION_MAJOR, 3);
  glfw.Window.hint(glfw.CONTEXT_VERSION_MINOR, 2);
  glfw.Window.hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE);
  glfw.Window.hint(glfw.OPENGL_FORWARD_COMPAT, true);
  glfw.Window.hint(glfw.RESIZABLE, false);
  glfw.Window.hint(glfw.SAMPLES, 4);

  window = glfw.context.current = new glfw.Window(1024, 768, scriptArgs[0]);

  let context = glfw.context;
  console.log('context', context);

  const { position, size } = window;

  Object.assign(context, {
    begin(color = nvg.RGB(255, 255, 255)) {
      Clear(color);
      nvg.BeginFrame(...size, 1);
    },
    end() {
      nvg.EndFrame();
      window.swapBuffers();
      glfw.poll();
    },
  });

  Object.assign(window, {
    handleSize(width, height) {
      console.log('resized', { width, height });
    },
    handleKey(keyCode) {
      let charCode = keyCode & 0xff;
      console.log(`handleKey`, { keyCode: '0x' + keyCode.toString(16), charCode, char: String.fromCharCode(charCode) });
      let char = String.fromCodePoint(charCode);

      let handler = { '\x00': () => (running = false), Q: () => (running = false) }[char];
      if(handler) handler();
    },
    handleCharMods(char, mods) {
      console.log(`handleCharMods`, { char, mods });
    },
    handleMouseButton(button, action, mods) {
      console.log(`handleMouseButton`, { button, action, mods });
    },
    handleCursorPos(x, y) {
      //console.log(`handleCursorPos`, { x, y });
    },
  });

  nvg = CreateGL3(STENCIL_STROKES | ANTIALIAS | DEBUG);

  const { width, height } = size;
  const { x, y } = position;

  let pixels;
  let imgId = nvg.CreateImage('Architektur.png', 0);
  let img2Id = nvg.CreateImage('Muehleberg.png', 0);

  console.log(`main`, { imgId, img2Id });

  let img2Sz = nvg.ImageSize(img2Id);
  let imgSz = nvg.ImageSize(imgId);

  while((running &&= !window.shouldClose)) {
    let time = +new Date() / 1000;
    let index = Math.floor((time * 360) / 30);
    let color = nvg.RGB(30, 30, 30);

    context.begin(color);

    let m = nvg.CurrentTransform();
    let t = nvg.Transform.Translate(10, 20);
    let s = nvg.Transform.Scale(3, 3);

    let p = nvg.Transform.Multiply(m, t, s);

    let center = new glfw.Position(size.width / 2, size.height / 2);
    let imgSz = new glfw.Position(img2Sz.width * -1, img2Sz.height * -1);
    let imgSz_2 = new glfw.Position(img2Sz.width * -0.5, img2Sz.height * -0.5);
    let phi = a => ((a % 360) / 180) * Math.PI;
    let vec = (w, h, angle = phi(i)) => [Math.cos(angle) * w, Math.sin(angle) * h]; /*.map(n => n * radius)*/

    function Planet(radius, stroke, fill, getAngle, getPrecession, getPosition) {
      return Object.assign(this, {
        radius,
        stroke,
        fill,
        get angle() {
          return getAngle();
        },
        get precession() {
          return getPrecession();
        },
        get position() {
          return RotatePoint(...getPosition(this.angle), this.precession);
        },
        draw() {
          nvg.Save();
          nvg.Translate(...center);
          nvg.Translate(...this.position);
          DrawCircle(this.radius, this.stroke, this.fill);
          nvg.Restore();
        },
        get zpos() {
          const [x, y] = this.position;

          return y;
        },
      });
    }

    let planets = [
      new Planet(
        50,
        nvg.RGB(255, 255, 224),
        nvg.RGBA(255, 192, 0, 255),
        () => phi(i + 240),
        () => phi(i * 0.01),
        a => vec(20, 10, a),
      ),
      new Planet(
        20,
        nvg.RGB(255, 180, 180),
        nvg.RGBA(255, 0, 0, 0.8 * 255),
        () => phi(i),
        () => phi(i * -0.01),
        a => vec(300, 100, a),
      ),
      new Planet(
        30,
        nvg.RGB(160, 220, 255),
        nvg.RGBA(0, 120, 255, 0.8 * 255),
        () => phi(i * 0.8 + 120),
        () => phi(i * 0.02),
        a => vec(180, 40, a),
      ),
      new Planet(
        10,
        nvg.RGB(220, 160, 255),
        nvg.RGBA(120, 0, 255, 0.8 * 255),
        () => phi(i * 0.4 - 120),
        () => phi(i * 0.001),
        a => vec(320, 200, a),
      ),
    ];

    planets.sort((a, b) => a.zpos - b.zpos);

    for(let planet of planets) {
      planet.draw();
    }
    context.end();
    i++;
  }
}

const runMain = () => {
  try {
    main(...scriptArgs.slice(1));
    std.exit(0);
  } catch(error) {
    console.log('ERROR:', error);
  }
};

import('console').catch(runMain).then(({ Console }) => ((globalThis.console = new Console({ inspectOptions: {} })), runMain()));
