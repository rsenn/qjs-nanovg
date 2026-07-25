/*
 * emerald.js — "Emerald Run": a tiny Diamond-Rush-style dungeon crawler
 * demo built on engine2d.js. 640x640 canvas, 16x16 tiles.
 *
 * Run with:  qjsm --std emerald.js
 * Controls:  arrow keys / WASD to move, Escape to quit.
 *
 * This demo generates its tileset/sprites procedurally (see
 * makePlaceholderArt() below) so it runs with zero external assets. To use
 * real artwork instead, replace the loadTileset()/loadSprite() calls with
 * file paths, e.g.:
 *
 *   const tileset = Engine.loadTileset('assets/tiles.png', TILE, TILE);
 *   const player = Engine.loadSprite(world, 'assets/player.png', TILE, TILE, {
 *     idle: { frames: [0], fps: 1 },
 *     walk: { frames: [1, 2, 3, 2], fps: 8 },
 *   }, startX, startY, 'idle');
 *
 * See the bottom of this file / the project README for notes on
 * generating tilesets with AI art tools and on using Tiled (mapeditor.org)
 * to author real dungeon maps for Engine.loadWorld().
 */

import Engine from './engine2d.js';
import { canvas } from '../../lib/canvas2d.js';

const TILE = 16;
const VIEW = 640;

canvas.width = VIEW;
canvas.height = VIEW;
const ctx = canvas.getContext('2d');

/* ---------------------------------------------------- placeholder art */

function makeBuffer(w, h, paint) {
  const buf = new Uint8Array(w * h * 4);
  for(let y = 0; y < h; y++) {
    for(let x = 0; x < w; x++) {
      const [r, g, b, a] = paint(x, y);
      const i = (y * w + x) * 4;
      buf[i] = r;
      buf[i + 1] = g;
      buf[i + 2] = b;
      buf[i + 3] = a;
    }
  }
  return { width: w, height: h, pixels: buf };
}

// tileset: tile 0 = floor, tile 1 = wall (2 tiles side by side)
function makeTilesetPixels() {
  return makeBuffer(TILE * 2, TILE, (x, y) => {
    const tile = x < TILE ? 0 : 1;
    const lx = x % TILE;
    if(tile == 0) {
      const c = ((lx >> 2) + (y >> 2)) % 2 == 0 ? 96 : 108;
      return [c, c - 14, c - 28, 255];
    }
    const edge = lx == 0 || lx == TILE - 1 || y == 0 || y == TILE - 1;
    return edge ? [38, 38, 44, 255] : [66, 64, 72, 255];
  });
}

// player spritesheet: 4 frames, simple bobbing blob with alternating feet
function makePlayerPixels() {
  const frames = 4;
  return makeBuffer(TILE * frames, TILE, (x, y) => {
    const frame = Math.floor(x / TILE);
    const lx = x % TILE - TILE / 2;
    const bob = [0, -1, 0, 1][frame];
    const ly = y - TILE / 2 - bob;
    const bodyDist = Math.sqrt(lx * lx + (ly * 0.85) * (ly * 0.85));
    if(bodyDist < 5.5) return [220, 60, 50, 255]; // torso/head, red tunic
    if(bodyDist < 6.5) return [140, 30, 25, 255]; // outline
    const footSide = frame % 2 == 0 ? -1 : 1;
    if(y > TILE - 4 && Math.abs(x % TILE - (TILE / 2 + footSide * 3)) < 2) return [70, 45, 20, 255]; // feet
    return [0, 0, 0, 0];
  });
}

// emerald pickup: a simple green diamond
function makeEmeraldPixels() {
  return makeBuffer(TILE, TILE, (x, y) => {
    const dx = Math.abs(x - TILE / 2 + 0.5);
    const dy = Math.abs(y - TILE / 2 + 0.5);
    if(dx + dy < 5) return [40, 220, 140, 255];
    if(dx + dy < 6.5) return [20, 140, 90, 255];
    return [0, 0, 0, 0];
  });
}

/* ---------------------------------------------------------- dungeon map */

function generateDungeon(cols, rows, steps) {
  const grid = Array.from({ length: rows }, () => Array(cols).fill(1));
  let x = cols >> 1,
    y = rows >> 1;
  grid[y][x] = 0;
  const dirs = [
    [1, 0],
    [-1, 0],
    [0, 1],
    [0, -1],
  ];
  for(let i = 0; i < steps; i++) {
    const [dx, dy] = dirs[(Math.random() * 4) | 0];
    x = Math.min(cols - 2, Math.max(1, x + dx));
    y = Math.min(rows - 2, Math.max(1, y + dy));
    grid[y][x] = 0;
    if(Math.random() < 0.3) grid[Math.min(rows - 2, y + 1)][x] = 0;
    if(Math.random() < 0.3) grid[y][Math.min(cols - 2, x + 1)] = 0;
  }
  return grid;
}

function openTiles(grid) {
  const open = [];
  for(let y = 0; y < grid.length; y++) for(let x = 0; x < grid[0].length; x++) if(grid[y][x] == 0) open.push([x, y]);
  return open;
}

/* --------------------------------------------------------------- setup */

const COLS = 50,
  ROWS = 50;
const grid = generateDungeon(COLS, ROWS, 2200);
const open = openTiles(grid);

const tileset = Engine.loadTileset(makeTilesetPixels(), TILE, TILE);
const world = Engine.loadWorld(tileset, grid, { solid: [1] });

const [startX, startY] = open[(open.length / 2) | 0];
const player = Engine.loadSprite(
  world,
  makePlayerPixels(),
  TILE,
  TILE,
  {
    idle: { frames: [0], fps: 1 },
    walk: { frames: [0, 1, 0, 2], fps: 8 },
  },
  startX * TILE,
  startY * TILE,
  'idle',
);

const GEM_COUNT = 20;
const gems = [];
const gemImage = Engine.createImage(makeEmeraldPixels(), TILE, TILE);
for(const [gx, gy] of shuffle(open.filter(([x, y]) => x != startX || y != startY)).slice(0, GEM_COUNT)) {
  gems.push(
    Engine.loadSprite(world, gemImage, TILE, TILE, { idle: { frames: [0], fps: 1 } }, gx * TILE, gy * TILE, 'idle'),
  );
}

function shuffle(arr) {
  for(let i = arr.length - 1; i > 0; i--) {
    const j = (Math.random() * (i + 1)) | 0;
    [arr[i], arr[j]] = [arr[j], arr[i]];
  }
  return arr;
}

/* ------------------------------------------------------------- controls */

let score = 0;

window.addEventListener('keydown', e => {
  if(e.key == 'Escape') window.close();
});

const SPEED = 90; // px/sec

function updatePlayer(dt) {
  let dx = 0,
    dy = 0;
  if(Engine.Input.isDown('ArrowLeft') || Engine.Input.isDown('a')) dx -= 1;
  if(Engine.Input.isDown('ArrowRight') || Engine.Input.isDown('d')) dx += 1;
  if(Engine.Input.isDown('ArrowUp') || Engine.Input.isDown('w')) dy -= 1;
  if(Engine.Input.isDown('ArrowDown') || Engine.Input.isDown('s')) dy += 1;

  if(dx || dy) {
    const len = Math.hypot(dx, dy);
    dx /= len;
    dy /= len;
    const step = (SPEED * dt) / 1000;
    const nx = player.x + dx * step;
    const ny = player.y + dy * step;
    if(!world.isSolidAt(nx, player.y, TILE, TILE)) player.x = nx;
    if(!world.isSolidAt(player.x, ny, TILE, TILE)) player.y = ny;
    if(dx != 0) player.flipX = dx < 0;
    player.play('walk');
  } else {
    player.play('idle');
  }

  for(let i = gems.length - 1; i >= 0; i--) {
    const gem = gems[i];
    if(Math.abs(gem.x - player.x) < TILE * 0.7 && Math.abs(gem.y - player.y) < TILE * 0.7) {
      world.removeEntity(gem);
      gems.splice(i, 1);
      score++;
    }
  }
}

/* ---------------------------------------------------------------- loop */

let lastTs = 0;

function loop(ts) {
  const dt = lastTs ? Math.min(50, ts - lastTs) : 16;
  lastTs = ts;

  updatePlayer(dt);
  world.update(dt);
  world.centerCameraOn(player.x + TILE / 2, player.y + TILE / 2, VIEW, VIEW);

  ctx.clearRect(0, 0, VIEW, VIEW);
  world.render(ctx, VIEW, VIEW);

  ctx.font = '18px sans-serif';
  ctx.fillStyle = '#ffe066';
  ctx.fillText(`Emeralds: ${score} / ${GEM_COUNT}`, 12, 26);

  requestAnimationFrame(loop);
}

requestAnimationFrame(loop);
