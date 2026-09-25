import { asyncify } from './asyncify.js';
import { writeFile } from './nanovg.js';

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
    const font = /\.ttf$/i.test(lit);
    if(!font && !/\.(png|jpe?g|gif|bmp)$/i.test(lit)) return;

    const data = await bytesOf(font ? `./fonts/${lit.slice(lit.lastIndexOf('/') + 1)}` : `./${lit}`);
    if(data) writeFile(lit, data);
  }),
);

const url = URL.createObjectURL(new Blob([code], { type: 'text/javascript' }));
await import(url);
