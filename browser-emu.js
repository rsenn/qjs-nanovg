import { Parser } from 'dom';
import { EventTarget } from 'events';
import * as glfw from 'glfw';

// 1. Initialize a valid DOM Document using the 'dom' Parser module
export const document = new Parser().parseFromString(`
  <html>
    <head>
      <title>QuickJS Browser Emulator</title>
    </head>
    <body>
      <canvas id="canvas" width="1024" height="768"></canvas>
    </body>
  </html>
`);

// 2. Setup the global `window` object extending EventTarget
class WindowProxy extends EventTarget {
  constructor() {
    super();
    this.innerWidth = 1024;
    this.innerHeight = 768;
    this.devicePixelRatio = 1;
  }

  requestAnimationFrame(callback) {
    // Map requestAnimationFrame timing to the event loop tick / performance counter
    return setTimeout(() => callback(glfw.getTime() * 1000), 16);
  }

  cancelAnimationFrame(id) {
    clearTimeout(id);
  }
}

export const window = (globalThis.window = new WindowProxy());
globalThis.document = document;

// 3. Create a stateful Canvas element class backed by EventTarget
class CanvasElement extends EventTarget {
  constructor(node) {
    super();
    this._node = node;
    this.width = 1024;
    this.height = 768;
    this.style = {};
  }

  getBoundingClientRect() {
    return {
      left: 0,
      top: 0,
      width: this.width,
      height: this.height,
      right: this.width,
      bottom: this.height,
    };
  }

  getContext(type) {
    if(type === 'webgl' || type === 'webgl2' || type === 'experimental-webgl') {
      // Returns a context stub; actual rendering is handled via qjs-glfw + NanoVG context current
      return { canvas: this };
    }
    return null;
  }
}

// Extract or fallback to the canvas element from the parsed document
const originalGetElementById = document.getElementById?.bind(document);
const parsedCanvasNode = originalGetElementById ? originalGetElementById('canvas') : document.querySelector?.('canvas');

export const canvas = new CanvasElement(parsedCanvasNode);

// Override getElementById so standard queries for 'id="canvas"' return our interactive canvas instance
if(document.getElementById) {
  document.getElementById = id => {
    if(id === 'canvas') return canvas;
    return originalGetElementById(id);
  };
}

// 4. Emulated Browser Window Bridge (Wraps qjs-glfw and forwards events to DOM elements)
export class EmulatedBrowserWindow {
  constructor(width = 1024, height = 768, title = 'QuickJS Emulated Browser') {
    glfw.Window.defaultHints();
    this.glfwWindow = new glfw.Window(width, height, title);
    this.glfwWindow.makeContextCurrent();

    // Sync dimensions across canvas and window globals
    canvas.width = width;
    canvas.height = height;
    window.innerWidth = width;
    window.innerHeight = height;

    this._setupEventBridge();
  }

  _setupEventBridge() {
    const win = this.glfwWindow;

    // Helper to calculate relative cursor positions
    const getPos = (x, y) => ({
      clientX: x,
      clientY: y,
      offsetX: x,
      offsetY: y,
      target: canvas,
    });

    win.handleCursorPos = (x, y) => {
      canvas.dispatchEvent(Object.assign(new Event('mousemove'), getPos(x, y)));
    };

    win.handleMouseButton = (button, action) => {
      const type = action === 1 ? 'mousedown' : 'mouseup';
      const event = Object.assign(new Event(type), { button, ...getPos(0, 0) });
      canvas.dispatchEvent(event);
    };

    win.handleKey = (key, scancode, action) => {
      if(action === 0) return; // keyup can be handled similarly if needed
      const event = Object.assign(new Event('keydown'), { keyCode: key, target: window });
      window.dispatchEvent(event);
    };
  }

  get shouldClose() {
    return this.glfwWindow.shouldClose;
  }

  swapBuffers() {
    this.glfwWindow.swapBuffers();
  }

  destroy() {
    this.glfwWindow.destroy();
  }
}

// Export event loop poller
export async function poll() {
  await glfw.poll();
}
