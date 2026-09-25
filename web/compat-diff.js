// node web/compat-diff.js native.json web.json
import { readFileSync } from 'node:fs';

const [a, b] = process.argv.slice(2).map(f => JSON.parse(readFileSync(f, 'utf8')));
const EXTRA_EXPORTS = ['writeFile'];
const TOLERANCE = 1e-5;
let bad = 0;

const report = (path, x, y) => {
  bad++;
  console.log(`${path}: native=${JSON.stringify(x)} web=${JSON.stringify(y)}`);
};

function compare(x, y, path) {
  if(typeof x == 'number' && typeof y == 'number') {
    if(Math.abs(x - y) > TOLERANCE * Math.max(1, Math.abs(x))) report(path, x, y);
  } else if(x && y && typeof x == 'object' && typeof y == 'object') {
    for(const k of new Set([...Object.keys(x), ...Object.keys(y)])) {
      if(path == '.describe.exports' && EXTRA_EXPORTS.includes(k) && !(k in x)) continue;
      if(!(k in x) || !(k in y)) report(`${path}.${k}`, x[k], y[k]);
      else compare(x[k], y[k], `${path}.${k}`);
    }
  } else if(x !== y) report(path, x, y);
}

compare(a, b, '');
console.log(bad ? `${bad} difference(s)` : 'identical');
process.exit(bad ? 1 : 0);
