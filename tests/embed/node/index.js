// FFI-9 Node consumer: drives Strut through the N-API addon + shared embedding library.
// Exercises context create/destroy, source load + reload, int invocation, string round-trip,
// binary Buffer round-trip (embedded NUL + high bit), structured checked errors, a borrowed
// JS callback, retained callback + reload, BUSY destruction and explicit release.
const addon = require('./build/Release/strut_embed_addon.node');
const assert = require('assert');

const SRC = [
  'export "C" function add(int_32 a, int_32 b) -> int_32 { return a + b; }',
  'export "C" function greet(string name) -> string { return "hi " + name; }',
  'export "C" function echo_bytes(bytes b) -> bytes { return b; }',
  'export "C" function apply_cb(function<(int_32)->int_32> f, int_32 v) -> int_32 { return f(v) + f(v + 1); }',
  'error EmbedErr { string message; }',
  'export "C" function risky(int_32 x) -> int_32 : EmbedErr { if (x < 0) { throw EmbedErr { message: "boom" }; } return x; }',
  'export "C" function make_rc(int_32 base) -> retained_callback<(int_32)->int_32> { return retained_callback((int_32 x) => x + base); }',
].join('\n');

function main() {
  const ctx = addon.StrutOpen();
  assert(ctx, 'create');
  let rc = addon.StrutLoad(ctx, SRC);
  assert(rc === 0, 'load source: ' + rc);

  assert(addon.StrutInvoke(ctx, 'add', [20, 22]) === 42, 'add=42');

  const g = addon.StrutInvoke(ctx, 'greet', ['stranger']);
  assert(g instanceof Buffer && g.toString('utf8') === 'hi stranger', 'greet');

  const b = addon.StrutInvoke(ctx, 'echo_bytes', [Buffer.from([0x61, 0x00, 0x62, 0xff])]);
  assert(b instanceof Buffer && b.length === 4 && b[0] === 0x61 && b[1] === 0 && b[2] === 0x62 && b[3] === 0xff, 'bytes exact');

  let checked = null;
  try { addon.StrutInvoke(ctx, 'risky', [-1]); } catch (e) { checked = e; }
  assert(checked && checked.category === 4 && checked.type === 'EmbedErr' && checked.msg === 'boom', 'checked error');

  const base = 100;
  const cb = (x) => x + base;
  assert(addon.StrutInvoke(ctx, 'apply_cb', [cb, 5]) === 211, 'apply_cb=' + addon.StrutInvoke(ctx, 'apply_cb', [cb, 5]));

  const rcHandle = addon.StrutInvoke(ctx, 'make_rc', [100]);
  assert(rcHandle && typeof rcHandle.retained === 'number', 'make_rc');
  assert(addon.StrutRetained(ctx, rcHandle.retained, [5]) === 105, 'retained before reload');

  assert(addon.StrutLoad(ctx, 'export "C" function twice(int_32 x) -> int_32 { return x * 2; }') === 0, 'reload');
  assert(addon.StrutRetained(ctx, rcHandle.retained, [5]) === 105, 'retained after reload');

  assert(addon.StrutClose(ctx) !== 0, 'BUSY destroy expected while retained outstanding');
  addon.StrutFree(ctx, rcHandle.retained);
  assert(addon.StrutClose(ctx) === 0, 'destroy after release');

  console.log('node consumer ok');
}

main();