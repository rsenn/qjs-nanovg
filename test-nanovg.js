import * as glfw from 'glfw';
import { CreateGL3, STENCIL_STROKES, ANTIALIAS, DEBUG, DegToRad, ReadPixels, CreateImageFromHandleGL3, ImageHandleGL3, IMAGE_NODELETE, CreateFramebuffer, BindFramebuffer, DeleteFramebuffer, Transform, TransformPoint, RGB, RGBA, } from 'nanovg';

const C = console.config({ compact: true });

const log = (...args) => console.log(C, ...args);

let window;

export function DrawCircle(radius, stroke = RGB(255, 255, 255), fill = RGBA(255, 0, 0, 96)) {
  nvg.BeginPath();
  nvg.StrokeColor(stroke);
  nvg.StrokeWidth(3);
  if(fill) nvg.FillColor(fill);
  nvg.Circle(0, 0, radius);
  if(fill) nvg.Fill();
  nvg.Stroke();
}

export function Clear(color = RGB(0, 0, 0)) {
  const { size } = window;

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

  const context = glfw.context;
  log('context', context);

  const { size } = window;
  const { width, height } = size;

  let paused = false;
  let rate = 1.0;

  Object.assign(context, {
    begin(color = RGB(255, 255, 255)) {
      Clear(color);
      nvg.BeginFrame(width, height, 1);
    },
    end() {
      nvg.EndFrame();
      window.swapBuffers();
      glfw.poll();
    },
  });

  Object.assign(window, {
    handleSize(width, height) {
      log('resized', { width, height });
    },
    handleKey(keyCode, scancode, action, mods) {
      let char = String.fromCodePoint(keyCode);

      log(`handleKey`, console.config({ numberBase: 16 }), {
        keyCode,
        char,
        scancode,
        action,
        mods,
      });

      if(action) {
        let handler = {
          [glfw.KEY_ESCAPE]: () => (running = false),
          [glfw.KEY_Q]: () => (running = false),
          [glfw.KEY_LEFT]: () => ((paused = true), (i -= rate)),
          [glfw.KEY_RIGHT]: () => ((paused = true), (i += rate)),
          [glfw.KEY_UP]: () => (i = 0),
          [glfw.KEY_DOWN]: () => (i = 0),
        }[keyCode];

        if(handler) return handler();
      }
    },
    handleCharMods(charCode, mods) {
      let char = String.fromCodePoint(charCode);
      log(`handleCharMods`, { charCode, char, mods });

      let handler = {
        ' ': () => (paused = !paused),
        '+': () => (rate += 0.2),
        '-': () => (rate -= 0.2),
      }[char];

      if(handler) handler();
    },
    handleMouseButton(button, action, mods) {
      log(`handleMouseButton`, { button, action, mods });
    },
    /*handleCursorPos(x, y) {
      log(`handleCursorPos`, { x, y });
    },*/
  });

  const nvg = CreateGL3(STENCIL_STROKES | ANTIALIAS);

  Object.assign(globalThis, { nvg, glfw, Transform, TransformPoint, RGB, RGBA });

  const images = [new Image('Architektur.png', 0), new Image('Muehleberg.png', 0)];

  function Image(path) {
    const id = nvg.CreateImage(path, 0);
    const [width, height] = nvg.ImageSize(id);
    const pattern = nvg.ImagePattern(0, 0, width, height, 0, id, 1);

    return {
      id,
      width,
      height,
      draw() {
        nvg.Save();
        nvg.Translate(-width / 2, -height / 2);
        nvg.BeginPath();
        nvg.Rect(0, 0, width, height);
        nvg.FillPaint(pattern);
        nvg.Fill();
        nvg.Restore();
      },
    };
  }

  /* let img3Id = nvg.CreateImageRGBA(width, height, 0, (imgBuf = new ArrayBuffer(width * height)));*/

  log(`main`, { images });

  /*const fb = CreateFramebuffer(nvg, width, height, 0);
  const { fbo, rbo, texture, image } = fb;
  console.log('fb', fb, { fbo, rbo, texture, image });
  BindFramebuffer(fb);
  console.log('fb', fb, { fbo, rbo, texture, image });*/

  while((running &&= !window.shouldClose)) {
    let time = +new Date() / 1000;
    let index = Math.floor((time * 360) / 30);
    let color = RGB(30, 30, 30);

    context.begin(color);

    let m = nvg.CurrentTransform();
    let t = Transform.Translate(10, 20);
    let s = Transform.Scale(3, 3);
    let p = Transform.Multiply(m, t, s);

    let center = [width / 2, height / 2];
    let phi = a => DegToRad(a % 360);
    //let vec = (w, h, angle = phi(i)) => [Math.cos(angle) * w, Math.sin(angle) * h];

    let vec =(x,y,angle = phi(i)) => Transform.Rotate(angle).TransformPoint(x,y);

    function Planet(radius, stroke, fill, getAngle, getPrecession, [x, y]) {
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
          return vec(x, y, this.angle);
        },
        draw() {
          nvg.Save();
          nvg.Rotate(this.precession);
          nvg.Translate(...this.position);
          DrawCircle(this.radius, this.stroke, this.fill);
          nvg.Restore();
        },
        drawOrbit() {
          nvg.Save();
          nvg.Rotate(this.precession);
          nvg.BeginPath();
          nvg.StrokeColor(stroke);
          nvg.FillColor(fill);
          nvg.StrokeWidth(1);
          nvg.Ellipse(0, 0, x, y);
          nvg.Stroke();
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
        RGB(255, 255, 224),
        RGBA(255, 192, 0, 255),
        () => phi(i + 240),
        () => phi(i * 0.01),
        [20, 10],
      ),
      new Planet(
        20,
        RGB(255, 180, 180),
        RGBA(255, 0, 0, 0.8 * 255),
        () => phi(i),
        () => phi(i * -0.01),
        [300, 100],
      ),
      new Planet(
        30,
        RGB(160, 220, 255),
        RGBA(0, 120, 255, 0.8 * 255),
        () => phi(i * 0.8 + 120),
        () => phi(i * 0.02),
        [180, 40],
      ),
      new Planet(
        10,
        RGB(220, 160, 255),
        RGBA(120, 0, 255, 0.8 * 255),
        () => phi(i * 0.4 - 120),
        () => phi(i * 0.001),
        [320, 200],
      ),
    ];

    planets.sort((a, b) => a.zpos - b.zpos);

    nvg.Save();
    nvg.Translate(...center);

    /*nvg.Save();
    nvg.Translate(...vec(width/2 - images[0].height*0.1, height/2 - images[0].height*0.1, i * 0.01 + -Math.PI/2));
    nvg.Scale(0.2, 0.2);
    nvg.Rotate(i * 0.01);

    images[0].draw();
    nvg.Restore();*/

    for(let planet of planets) planet.drawOrbit();

    for(let planet of planets) {
      planet.draw();
    }

    nvg.Restore();

    context.end();

    if(!paused) i += rate;
  }

  /*console.log('imgBuf', imgBuf);

  let fbimg = CreateImageFromHandleGL3(nvg, fb.texture, width, height, 0);
  console.log('fbimg', fbimg);*/

  const pixels = ReadPixels(width, height);

  console.log('pixels', pixels);

  window.destroy();
}

main(...scriptArgs.slice(1));
