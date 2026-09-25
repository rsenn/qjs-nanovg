import { parse } from './acorn.mjs';

const FUNCTIONS = new Set(['FunctionDeclaration', 'FunctionExpression', 'ArrowFunctionExpression']);

const calleeName = ({ callee }) => (callee.type == 'Identifier' ? callee.name : callee.type == 'MemberExpression' && !callee.computed ? callee.property.name : null);

// The scripts drive their frames with a blocking `while(...) { glfw.poll(); }`. A browser only presents a
// frame when the script yields, so every function that (transitively, by name) calls poll() becomes async
// and its call sites get an `await`. Also returns the string literals so the caller can preload files.
export function asyncify(source) {
  const ast = parse(source, { ecmaVersion: 'latest', sourceType: 'module' });
  const funcs = [];
  const calls = [];
  const literals = [];
  const stack = [];

  const nameOf = (fn, parent) => fn.id?.name ?? (parent?.type == 'Property' || parent?.type == 'MethodDefinition' ? parent.key.name : parent?.type == 'VariableDeclarator' ? parent.id.name : null);

  (function visit(node, parent) {
    const isFn = FUNCTIONS.has(node.type);
    if(isFn) {
      const info = { node, parent, name: nameOf(node, parent), async: node.async };
      funcs.push(info);
      stack.push(info);
    }
    if(node.type == 'CallExpression') calls.push({ node, fn: stack[stack.length - 1] });
    if(node.type == 'Literal' && typeof node.value == 'string') literals.push(node.value);

    for(const key in node) {
      const v = node[key];
      if(Array.isArray(v)) v.forEach(c => c && typeof c.type == 'string' && visit(c, node));
      else if(v && typeof v.type == 'string') visit(v, node);
    }
    if(isFn) stack.pop();
  })(ast, null);

  const names = new Set(['poll']);
  for(let changed = true; changed; ) {
    changed = false;
    for(const { node, fn } of calls) {
      const name = calleeName(node);
      if(fn && name && names.has(name) && !fn.async) {
        fn.async = true;
        changed = true;
        if(fn.name) names.add(fn.name);
      }
    }
  }

  const edits = [];
  for(const fn of funcs) {
    if(!fn.async || fn.node.async) continue;
    const method = fn.parent && (fn.parent.type == 'MethodDefinition' || (fn.parent.type == 'Property' && fn.parent.method));
    edits.push([method ? fn.parent.start : fn.node.start, 'async ']);
  }
  for(const { node } of calls) {
    const name = calleeName(node);
    if(name && names.has(name)) edits.push([node.start, '(await '], [node.end, ')']);
  }

  edits.sort((a, b) => b[0] - a[0]);
  let code = source;
  for(const [pos, text] of edits) code = code.slice(0, pos) + text + code.slice(pos);

  return { code, literals };
}
