import { asyncify } from './asyncify.js';
import { assets } from './nanovg.js';

const name = new URLSearchParams(location.search).get('example') ?? 'planets';
const source = await (await fetch(`./examples/${name}.js`)).text();
const { code, literals } = asyncify(source);

const CFG = Symbol('console.config');
const log = console.log;
console.log = (...args) => log(...args.filter(a => !a?.[CFG]));
console.config = opts => ({ [CFG]: true, ...opts });
globalThis.scriptArgs = [`${name}.js`];

const bytesOf = async url => {
  const res = await fetch(url);
  return res.ok ? res.arrayBuffer() : null;
};

await Promise.all(
  literals.map(async lit => {
    const file = lit.slice(lit.lastIndexOf('/') + 1);
    if(/\.ttf$/i.test(lit) && !assets.fonts.has(file)) {
      const data = await bytesOf(`./fonts/${file}`);
      if(data) assets.fonts.set(file, data);
    } else if(/\.(png|jpe?g|gif|bmp)$/i.test(lit)) {
      const data = await bytesOf(`./${lit}`);
      if(!data) return;
      const bitmap = await createImageBitmap(new Blob([data]));
      const cx = new OffscreenCanvas(bitmap.width, bitmap.height).getContext('2d');
      cx.drawImage(bitmap, 0, 0);
      assets.images.set(lit, { width: bitmap.width, height: bitmap.height, data: cx.getImageData(0, 0, bitmap.width, bitmap.height).data });
    }
  }),
);

const url = URL.createObjectURL(new Blob([code], { type: 'text/javascript' }));
await import(url);
