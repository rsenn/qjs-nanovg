import * as glfw from 'glfw';
//import { glClear, glClearColor, glViewport, GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT } from './gl.js';
import * as nvg from 'nanovg';

let window;

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

export function DrawCircle(pos, radius, stroke = nvg.RGB(255,255,255), fill = nvg.RGBA(255,0,0,96)) {
  nvg.Save();
  nvg.Translate(...pos);
  nvg.BeginPath();
  nvg.StrokeColor(stroke);
  nvg.StrokeWidth(5);
  if(fill)
  nvg.FillColor(fill);
  nvg.Circle(0, 0, radius);
  if(fill) nvg.Fill();
  nvg.Stroke();
  nvg.Restore();
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
  /* globalThis.console = new Console({
    inspectOptions: {
      maxStringLength: 200,
      maxArrayLength: 10,
      breakLength: Infinity,
      compact: 2,
      depth: 10
    }
  });*/

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
      console.log('begin', color);
      Clear(color);
      nvg.BeginFrame(...size, 1);
    },
    end() {
      nvg.EndFrame();
      window.swapBuffers();
      glfw.poll();
    }
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
    }
  });

  nvg.CreateGL3(nvg.STENCIL_STROKES | nvg.ANTIALIAS | nvg.DEBUG);

  const { width, height } = size;
  const { x, y } = position;

  let pixels;
  let imgId = nvg.CreateImage('Architektur.png', 0);
  let img2Id = nvg.CreateImage('Muehleberg.png', 0);

  let img2Sz = nvg.ImageSize(img2Id);
  let imgSz = nvg.ImageSize(imgId);

  while((running &&= !window.shouldClose)) {
    let time = +new Date() / 1000;
    let index = Math.floor((time * 360) / 30);
    let color = nvg.RGB(30, 30, 30);

    context.begin(color);

    let m = nvg.CurrentTransform();
    let t = nvg.TransformTranslate([], 10, 20);
    let s = nvg.TransformScale([], 3, 3);

    let p = nvg.TransformMultiply(nvg.TransformMultiply(m, t), s);

    // let pattern = nvg.ImagePattern(0, 0, ...img2Sz, 0, img2Id, 1);

    let center = new glfw.Position(size.width / 2, size.height / 2);
    let imgSz = new glfw.Position(img2Sz.width * -1, img2Sz.height * -1);
    let imgSz_2 = new glfw.Position(img2Sz.width * -0.5, img2Sz.height * -0.5);

    nvg.Save();

    DrawCircle(center, 100, nvg.RGBA(255,192,0,255),nvg.RGBA(0,0,0,0));

    nvg.Translate(...center);
    nvg.Translate(...imgSz_2);

    let phi = ((i % 360) / 180) * Math.PI;
    let vec = [Math.cos(phi), Math.sin(phi)].map(n => n * 100);

    //  DrawImage(img2Id, vec);

    // nvg.Translate(...vec);
    //nvg.Translate(imgSz_2.x * -1, imgSz_2.y * -1);

    //nvg.Scale(0.5, 0.5);
    DrawCircle(new glfw.Position(...vec), 50, nvg.RGB(255,255,255), nvg.RGBA(255,0,0,0.8*255));
    //  DrawCircle(new glfw.Position(0, 0), 40);

    nvg.Restore();

    context.end();
    /*window.swapBuffers();
    glfw.poll();*/
    i++;
  }
}

const runMain = () => {
  try {
    main(scriptArgs.slice(1));
    std.exit(0);
  } catch(error) {
    console.log('ERROR:', error);
  }
};
import('console') .catch(runMain) .then(({ Console }) => ((globalThis.console = new Console({ inspectOptions: {} })), runMain()));
/*
try {
  main(...scriptArgs.slice(1));
} catch(error) {
  console.log('ERROR:',error.message);
   std.exit(1);
} finally {
 }
*/
