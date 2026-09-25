// node web/compat-node.js > web.json
import { collect } from './compat.js';
import * as ns from './nanovg.js';

console.log(JSON.stringify(collect(ns)));
