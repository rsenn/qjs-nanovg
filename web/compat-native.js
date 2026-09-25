// qjsm web/compat-native.js /path/to/nanovg.so > native.json
import { collect } from './compat.js';

console.log(JSON.stringify(collect(await import(scriptArgs[1]))));
