/*
 * engine2d.js — a minimal tile-based 2D sprite engine that runs unmodified
 * in either a real browser or under qjsm (via lib/canvas2d.js, a
 * Canvas2D emulation over nanovg/glfw). Suitable for a top-down/2.5D tile
 * world with animated sprites: dungeon crawlers, platformers-on-a-grid,
 * that kind of thing.
 *
 * Public asset API (the "only 3 functions" surface):
 *   Engine.loadTileset(source, tileW, tileH)        -> Tileset
 *   Engine.loadWorld(tileset, mapData, opts)         -> World
 *   Engine.loadSprite(world, source, frameW, frameH, anims, x, y, anim) -> Sprite
 *
 * `source` for loadTileset/loadSprite is either a file path (string,
 * resolved via the DOM Image loader) or a procedural pixel buffer
 * `{ width, height, pixels: Uint8Array }` (RGBA, row-major) — handy for
 * placeholder art or runtime-generated textures. See Engine.createImage().
 *
 * World maps (`mapData` for loadWorld) can be:
 *   - a 2D array of tile ids: [[0,1,2,...], [...]]  (-1 / null = empty)
 *   - a flat array + {width,height}
 *   - a Tiled (mapeditor.org) JSON export object, or (qjsm only) a path to
 *     one — its first tile layer is used, tile ids are shifted down by 1
 *     (Tiled reserves 0 for "empty", this engine uses -1). In the browser,
 *     fetch() and JSON.parse() the export yourself and pass the object in.
 *
 * Rendering, animation, input and camera scrolling are handled by the
 * returned World/Sprite objects; see their methods below.
 *
 * Environment detection: under a real browser, window/document/Image/
 * requestAnimationFrame are native and this file needs nothing else. Under
 * qjsm, those are supplied by lib/canvas2d.js, which is only reachable
 * through 'std'/'nanovg'/'glfw'/'dom' — modules a browser doesn't have. So
 * both are loaded dynamically (only when NOT running in a browser) rather
 * than with a static import, which is what lets this exact file run in
 * either environment without edits.
 */

const inBrowser = typeof window != 'undefined' && typeof window.document != 'undefined' && typeof window.HTMLCanvasElement != 'undefined';

let nv = null; // qjsm only: the raw nanovg context, needed for CreateImageRGBA
let canvasEl = null;
let loadFileSync = null; // qjsm only: std.loadFile, for path-based loadWorld()

if(inBrowser) {
  canvasEl = document.querySelector('canvas') ?? document.getElementById('canvas');
  if(!canvasEl) throw new Error('engine2d: no <canvas> element found in the page; add one before importing engine2d.js');
} else {
  const std = await import('std');
  const Canvas2D = (await import('../../lib/canvas2d.js')).default;
  nv = Canvas2D.nvg;
  canvasEl = Canvas2D.canvas;
  loadFileSync = std.loadFile;
}

/* ---------------------------------------------------------------- images */

function createImage(source, w, h) {
  if(typeof source == 'string') {
    const img = new Image();
    img.src = source;
    return img;
  }

  if(source && source.pixels === undefined) return source; // already a loaded image/texture

  const { width = w, height = h, pixels } = source;

  if(inBrowser) {
    const off = document.createElement('canvas');
    off.width = width;
    off.height = height;
    off.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(pixels), width, height), 0, 0);
    return off;
  }

  const buffer = pixels.buffer ? pixels.buffer : pixels;
  const id = nv.CreateImageRGBA(width, height, 0, buffer);
  if(id <= 0) throw new Error('createImage: failed to create image from pixel buffer');
  return { width, height, _id: id };
}

/* --------------------------------------------------------------- tileset */

class Tileset {
  constructor(image, tileW, tileH) {
    this.image = image;
    this.tileW = tileW;
    this.tileH = tileH;
    this.cols = Math.max(1, Math.floor(image.width / tileW));
    this.rows = Math.max(1, Math.floor(image.height / tileH));
  }

  draw(ctx, id, dx, dy) {
    if(id == null || id < 0) return;
    const sx = (id % this.cols) * this.tileW;
    const sy = Math.floor(id / this.cols) * this.tileH;
    ctx.drawImage(this.image, sx, sy, this.tileW, this.tileH, dx, dy, this.tileW, this.tileH);
  }
}

function loadTileset(source, tileW, tileH) {
  return new Tileset(createImage(source, tileW, tileH), tileW, tileH);
}

/* ----------------------------------------------------------------- world */

function normalizeMap(mapData) {
  if(typeof mapData == 'string') {
    if(!loadFileSync) throw new Error('loadWorld: string map paths are qjsm-only; in the browser fetch() the JSON and pass the parsed object instead');
    mapData = JSON.parse(loadFileSync(mapData));
  }

  if(Array.isArray(mapData) && Array.isArray(mapData[0])) return mapData; // already 2D

  if(Array.isArray(mapData?.data) || Array.isArray(mapData?.layers?.[0]?.data)) {
    // Tiled JSON export: first tile layer, flat row-major data, 0 = empty.
    const layer = mapData.layers ? mapData.layers.find(l => l.type == 'tilelayer') : mapData;
    const { width, height, data } = layer;
    const grid = [];
    for(let y = 0; y < height; y++) grid.push(Array.from(data.slice(y * width, (y + 1) * width), v => v - 1));
    return grid;
  }

  if(Array.isArray(mapData) && Number.isInteger(mapData[0])) {
    throw new Error('loadWorld: flat tile array needs {width,height}; pass {data, width, height} instead');
  }

  if(mapData?.data && mapData?.width && mapData?.height) {
    const { data, width, height } = mapData;
    const grid = [];
    for(let y = 0; y < height; y++) grid.push(Array.from(data.slice(y * width, (y + 1) * width)));
    return grid;
  }

  throw new Error('loadWorld: unrecognized map data format');
}

class World {
  constructor(tileset, grid, opts = {}) {
    this.tileset = tileset;
    this.grid = grid;
    this.rows = grid.length;
    this.cols = grid[0].length;
    this.tileW = tileset.tileW;
    this.tileH = tileset.tileH;
    this.solid = opts.solid instanceof Set ? opts.solid : new Set(opts.solid ?? []);
    this.entities = [];
    this.camera = { x: 0, y: 0 };
  }

  get pixelWidth() {
    return this.cols * this.tileW;
  }
  get pixelHeight() {
    return this.rows * this.tileH;
  }

  tileAt(tx, ty) {
    return ty >= 0 && ty < this.rows && tx >= 0 && tx < this.cols ? this.grid[ty][tx] : -1;
  }

  isSolidTile(tx, ty) {
    const id = this.tileAt(tx, ty);
    return id < 0 || this.solid.has(id);
  }

  // true if a tileW x tileH box at pixel (x,y) overlaps any solid tile
  isSolidAt(x, y, w = this.tileW, h = this.tileH) {
    const tx0 = Math.floor(x / this.tileW);
    const ty0 = Math.floor(y / this.tileH);
    const tx1 = Math.floor((x + w - 1) / this.tileW);
    const ty1 = Math.floor((y + h - 1) / this.tileH);
    for(let ty = ty0; ty <= ty1; ty++) for (let tx = tx0; tx <= tx1; tx++) if(this.isSolidTile(tx, ty)) return true;
    return false;
  }

  addEntity(entity) {
    this.entities.push(entity);
    return entity;
  }

  removeEntity(entity) {
    const i = this.entities.indexOf(entity);
    if(i != -1) this.entities.splice(i, 1);
  }

  // keep the camera centered on (x,y), clamped to map bounds
  centerCameraOn(x, y, viewW, viewH) {
    this.camera.x = Math.max(0, Math.min(this.pixelWidth - viewW, x - viewW / 2));
    this.camera.y = Math.max(0, Math.min(this.pixelHeight - viewH, y - viewH / 2));
  }

  update(dt) {
    for(const entity of this.entities) entity.update?.(dt);
  }

  render(ctx, viewW, viewH) {
    const { x: camX, y: camY } = this.camera;

    const tx0 = Math.max(0, Math.floor(camX / this.tileW));
    const ty0 = Math.max(0, Math.floor(camY / this.tileH));
    const tx1 = Math.min(this.cols - 1, Math.ceil((camX + viewW) / this.tileW));
    const ty1 = Math.min(this.rows - 1, Math.ceil((camY + viewH) / this.tileH));

    for(let ty = ty0; ty <= ty1; ty++) for (let tx = tx0; tx <= tx1; tx++) this.tileset.draw(ctx, this.grid[ty][tx], tx * this.tileW - camX, ty * this.tileH - camY);

    // painter's algorithm: draw back-to-front by y (simple 2.5D depth)
    for(const entity of [...this.entities].sort((a, b) => a.y - b.y)) entity.render?.(ctx, camX, camY);
  }
}

function loadWorld(tileset, mapData, opts) {
  return new World(tileset, normalizeMap(mapData), opts);
}

/* ---------------------------------------------------------------- sprite */

class SpriteSheet {
  constructor(image, frameW, frameH, anims) {
    this.image = image;
    this.frameW = frameW;
    this.frameH = frameH;
    this.cols = Math.max(1, Math.floor(image.width / frameW));
    this.anims = anims;
  }

  frameRect(index) {
    return [(index % this.cols) * this.frameW, Math.floor(index / this.cols) * this.frameH, this.frameW, this.frameH];
  }
}

class Sprite {
  constructor(sheet, x, y, defaultAnim) {
    this.sheet = sheet;
    this.x = x;
    this.y = y;
    this.flipX = false;
    this.visible = true;
    this.animName = null;
    this.frameIndex = 0;
    this.elapsed = 0;
    this.play(defaultAnim ?? Object.keys(sheet.anims)[0]);
  }

  get width() {
    return this.sheet.frameW;
  }
  get height() {
    return this.sheet.frameH;
  }

  play(name) {
    if(this.animName == name) return;
    if(!(name in this.sheet.anims)) throw new Error(`Sprite.play: unknown animation '${name}'`);
    this.animName = name;
    this.anim = this.sheet.anims[name];
    this.frameIndex = 0;
    this.elapsed = 0;
  }

  update(dt) {
    const { anim } = this;
    if(!anim || anim.frames.length < 2) return;
    this.elapsed += dt;
    const frameTime = 1000 / (anim.fps ?? 8);
    while(this.elapsed >= frameTime) {
      this.elapsed -= frameTime;
      this.frameIndex = (this.frameIndex + 1) % anim.frames.length;
    }
  }

  render(ctx, camX, camY) {
    if(!this.visible || !this.anim) return;
    const [sx, sy, w, h] = this.sheet.frameRect(this.anim.frames[this.frameIndex]);
    const dx = Math.round(this.x - camX);
    const dy = Math.round(this.y - camY);

    if(this.flipX) {
      ctx.save();
      ctx.translate(dx + w, dy);
      ctx.scale(-1, 1);
      ctx.drawImage(this.sheet.image, sx, sy, w, h, 0, 0, w, h);
      ctx.restore();
    } else {
      ctx.drawImage(this.sheet.image, sx, sy, w, h, dx, dy, w, h);
    }
  }
}

function loadSprite(world, source, frameW, frameH, anims, x = 0, y = 0, defaultAnim) {
  const sheet = new SpriteSheet(createImage(source, frameW, frameH), frameW, frameH, anims);
  const sprite = new Sprite(sheet, x, y, defaultAnim);
  if(world) world.addEntity(sprite);
  return sprite;
}

/* ----------------------------------------------------------------- input */

const Input = (() => {
  const keys = new Set();
  const mouse = { x: 0, y: 0, down: false };

  window.addEventListener('keydown', e => keys.add(e.key));
  window.addEventListener('keyup', e => keys.delete(e.key));
  canvasEl.addEventListener('mousemove', e => {
    mouse.x = e.clientX;
    mouse.y = e.clientY;
  });
  canvasEl.addEventListener('mousedown', () => (mouse.down = true));
  canvasEl.addEventListener('mouseup', () => (mouse.down = false));

  return {
    keys,
    mouse,
    isDown: key => keys.has(key),
  };
})();

/* ---------------------------------------------------------------- Engine */

const Engine = {
  canvas: canvasEl,
  createImage,
  loadTileset,
  loadWorld,
  loadSprite,
  Input,
  Tileset,
  World,
  SpriteSheet,
  Sprite,
};

export { canvasEl as canvas, createImage, loadTileset, loadWorld, loadSprite, Input, Tileset, World, SpriteSheet, Sprite };
export default Engine;
