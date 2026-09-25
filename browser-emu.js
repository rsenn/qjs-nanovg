import { Parser } from 'dom';
import { EventTarget } from 'events';
import * as glfw from 'glfw';
import { performance } from 'perf_hooks';

// Expose performance globally
globalThis.performance = performance;

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
    return setTimeout(() => callback(performance.now()), 16);
  }

  cancelAnimationFrame(id) {
    clearTimeout(id);
  }
}

export const window = new WindowProxy();

// 3. Implement requestAnimationFrame globally and export it
export function requestAnimationFrame(callback) {
  return window.requestAnimationFrame(callback);
}

export function cancelAnimationFrame(id) {
  return window.cancelAnimationFrame(id);
}

// Populate globalThis targets immediately on load
globalThis.window = window;
globalThis.document = document;
globalThis.requestAnimationFrame = requestAnimationFrame;
globalThis.cancelAnimationFrame = cancelAnimationFrame;

// 4. Create a stateful Canvas element class backed by EventTarget
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
    if (type === 'webgl' || type === 'webgl2' || type === 'experimental-webgl') {
      return { canvas: this };
    }
    return null;
  }
}

const originalGetElementById = document.getElementById?.bind(document);
const parsedCanvasNode = originalGetElementById ? originalGetElementById('canvas') : document.querySelector?.('canvas');

export const canvas = new CanvasElement(parsedCanvasNode);

if (document.getElementById) {
  document.getElementById = (id) => {
    if (id === 'canvas') return canvas;
    return originalGetElementById(id);
  };
}

// 5. Emulated Browser Window Bridge
export class EmulatedBrowserWindow {
  constructor(width = 1024, height = 768, title = 'QuickJS Emulated Browser') {
    glfw.Window.defaultHints();
    this.glfwWindow = new glfw.Window(width, height, title);
    this.glfwWindow.makeContextCurrent();

    canvas.width = width;
    canvas.height = height;
    window.innerWidth = width;
    window.innerHeight = height;

    this._setupEventBridge();
  }

  _setupEventBridge() {
    const win = this.glfwWindow;

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
      if (action === 0) return;
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

// Automatically spin up a default window and make its GL context current on module load
export const defaultWindow = new EmulatedBrowserWindow(1024, 768, 'QuickJS Emulated Browser');

// Export event loop poller
export async function poll() {
  await glfw.poll();
}

// Automatically load target script passed via command line arguments if available
const targetScript = scriptArgs[1]; // qjsm passes arguments after script name
if (targetScript) {
  import(targetScript).catch(err => {
    console.error(`Failed to load target script ${targetScript}:`, err);
  });
} else {
  // Fallback idle loop if no script provided
  async function keepAlive() {
    while (!defaultWindow.shouldClose) {
      await poll();
      await new Promise(r => setTimeout(r, 5));
    }
  }
  keepAlive();
}
