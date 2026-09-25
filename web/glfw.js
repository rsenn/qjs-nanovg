export const CONTEXT_VERSION_MAJOR = 0x22002;
export const CONTEXT_VERSION_MINOR = 0x22003;
export const OPENGL_PROFILE = 0x22008;
export const OPENGL_CORE_PROFILE = 0x32001;
export const OPENGL_FORWARD_COMPAT = 0x22006;
export const RESIZABLE = 0x20003;
export const SAMPLES = 0x2100d;

export const KEY_ESCAPE = 256;
export const KEY_RIGHT = 262;
export const KEY_LEFT = 263;
export const KEY_DOWN = 264;
export const KEY_UP = 265;
export const KEY_Q = 81;
export const KEY_S = 83;

const SPECIAL_KEYS = { Escape: KEY_ESCAPE, Enter: 257, Tab: 258, Backspace: 259, Delete: 261, ArrowRight: KEY_RIGHT, ArrowLeft: KEY_LEFT, ArrowDown: KEY_DOWN, ArrowUp: KEY_UP };
const BUTTONS = [0, 2, 1]; // DOM left/middle/right -> GLFW left/right/middle

const modsOf = e => (e.shiftKey ? 1 : 0) | (e.ctrlKey ? 2 : 0) | (e.altKey ? 4 : 0) | (e.metaKey ? 8 : 0);

export const context = { current: null };

export class Window {
  static hint() {}

  #off = [];

  constructor(width, height, title) {
    const canvas = (this.canvas = document.getElementById('canvas'));
    canvas.width = width;
    canvas.height = height;
    document.title = title;
    this.shouldClose = false;

    const canvasPos = e => {
      const r = canvas.getBoundingClientRect();
      return [((e.clientX - r.left) * canvas.width) / r.width, ((e.clientY - r.top) * canvas.height) / r.height];
    };
    const on = (target, type, fn) => {
      target.addEventListener(type, fn);
      this.#off.push(() => target.removeEventListener(type, fn));
    };

    on(canvas, 'mousemove', e => this.handleCursorPos?.(...canvasPos(e)));
    const button = (e, action) => {
      this.handleCursorPos?.(...canvasPos(e));
      this.handleMouseButton?.(BUTTONS[e.button] ?? e.button, action, modsOf(e));
    };
    on(canvas, 'mousedown', e => button(e, 1));
    on(window, 'mouseup', e => button(e, 0));
    on(canvas, 'contextmenu', e => e.preventDefault());
    on(window, 'keydown', e => {
      const key = SPECIAL_KEYS[e.key] ?? (e.key == ' ' ? 32 : e.key.length == 1 ? e.key.toUpperCase().codePointAt(0) : 0);
      this.handleKey?.(key, 0, e.repeat ? 2 : 1, modsOf(e));
      if(e.key.length == 1 && !e.ctrlKey && !e.metaKey) this.handleCharMods?.(e.key.codePointAt(0), modsOf(e));
    });
    on(window, 'keyup', e => {
      const key = SPECIAL_KEYS[e.key] ?? (e.key == ' ' ? 32 : e.key.length == 1 ? e.key.toUpperCase().codePointAt(0) : 0);
      this.handleKey?.(key, 0, 0, modsOf(e));
    });
  }

  get size() {
    const size = [this.canvas.width, this.canvas.height];
    size.width = size[0];
    size.height = size[1];
    return size;
  }

  swapBuffers() {}

  destroy() {
    this.#off.forEach(f => f());
    this.#off = [];
  }
}

// Frames only reach the screen when the script yields, so poll() has to be awaited.
export const poll = () => new Promise(resolve => requestAnimationFrame(() => resolve()));
