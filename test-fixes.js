import { DegToRad, HSL, HSLA, LerpRGBA, RadToDeg, RGB, RGBA, RGBAf, RGBf, Transform, TransformPoint, TransRGBA, TransRGBAf, } from 'nanovg';

let passed = 0;
let failed = 0;
const failures = [];

function assert(cond, msg) {
  if(cond) {
    passed++;
  } else {
    failed++;
    failures.push(msg);
    console.log('FAIL:', msg);
  }
}

function approx(a, b, eps = 1e-5) {
  return Math.abs(a - b) <= eps;
}

function arrApprox(a, b, eps = 1e-5) {
  if(a.length !== b.length) return false;
  for(let i = 0; i < a.length; i++) if(!approx(a[i], b[i], eps)) return false;
  return true;
}

function safe(name, fn) {
  try {
    fn();
  } catch(e) {
    failed++;
    failures.push(`${name} threw: ${e && e.message}`);
    console.log(`THROW in ${name}:`, e && e.message);
  }
}

/* ------------------------------------------------------------------ *
 * Group A — colour helpers (no GL context needed)                    *
 * ------------------------------------------------------------------ */
safe('RGB', () => {
  const c = RGB(255, 0, 0);
  assert(
    approx(c[0], 1) && approx(c[1], 0) && approx(c[2], 0) && approx(c[3], 1),
    'RGB(255,0,0) -> [1,0,0,1]',
  );
});

safe('RGBA', () => {
  const c = RGBA(0, 128, 255, 64);
  assert(
    approx(c[0], 0) &&
      approx(c[1], 128 / 255) &&
      approx(c[2], 1) &&
      approx(c[3], 64 / 255),
    'RGBA(0,128,255,64) -> [0, ~.502, 1, ~.251]',
  );
});

safe('RGBf', () => {
  const c = RGBf(0.25, 0.5, 0.75);
  assert(
    approx(c[0], 0.25) &&
      approx(c[1], 0.5) &&
      approx(c[2], 0.75) &&
      approx(c[3], 1),
    'RGBf(.25,.5,.75) -> [.25,.5,.75,1]',
  );
});

safe('RGBAf', () => {
  const c = RGBAf(0.25, 0.5, 0.75, 0.5);
  assert(
    approx(c[0], 0.25) &&
      approx(c[1], 0.5) &&
      approx(c[2], 0.75) &&
      approx(c[3], 0.5),
    'RGBAf(.25,.5,.75,.5) -> [.25,.5,.75,.5]',
  );
});

/* nvgjs_tocolor — this is the buggy one. LerpRGBA / TransRGBA{,f} all rely
   on it to unpack a NVGcolor from a Float32Array. */
safe('LerpRGBA 4-elem colour', () => {
  const c0 = RGBA(200, 0, 0, 255);
  const c1 = RGBA(0, 200, 0, 255);
  const mid = LerpRGBA(c0, c1, 0.5);
  assert(
    approx(mid[0], 100 / 255, 1e-3) &&
      approx(mid[1], 100 / 255, 1e-3) &&
      approx(mid[2], 0) &&
      approx(mid[3], 1),
    `LerpRGBA halfway between two RGBA -> got [${[...mid].join(',')}]`,
  );
});

safe('LerpRGBA repeated (pending-exception check)', () => {
  /* If nvgjs_tocolor leaves a pending exception on the ctx (which it does for
     3-element inputs), the *next* API call inherits the exception and throws.
     A tight loop of legal calls verifies no exception leaks between calls. */
  const c0 = RGBA(255, 0, 0, 255);
  const c1 = RGBA(0, 255, 0, 255);
  for(let i = 0; i < 32; i++) {
    const m = LerpRGBA(c0, c1, i / 32);
    if(!(m instanceof Float32Array && m.length === 4)) {
      throw new Error(`iteration ${i}: expected Float32Array(4), got ${m}`);
    }
  }
  assert(true, 'LerpRGBA in a loop keeps returning valid colours');
});

safe('TransRGBA', () => {
  const c = RGBA(255, 0, 0, 255);
  const t = TransRGBA(c, 64);
  assert(
    approx(t[0], 1) && approx(t[3], 64 / 255, 1e-3),
    `TransRGBA(red, 64) -> alpha ~= .251, got ${[...t].join(',')}`,
  );
});

safe('TransRGBAf', () => {
  const c = RGBA(255, 0, 0, 255);
  const t = TransRGBAf(c, 0.25);
  assert(
    approx(t[0], 1) && approx(t[3], 0.25, 1e-3),
    `TransRGBAf(red, .25) -> alpha=.25, got ${[...t].join(',')}`,
  );
});

safe('LerpRGBA with plain-Array colours (nvgjs_tocolor RGB fallback)', () => {
  /* 3-element input triggers nvgjs_tocolor's fallback: read 4 fails with a
     RangeError, alpha is set to 1, read 3 succeeds. Currently the RangeError
     from the failed 4-read is left pending on the ctx — the next call inherits
     it and throws. After the fix, both paths cleanly return. */
  const mid = LerpRGBA([1, 0, 0], [0, 1, 0], 0.5);
  assert(
    mid instanceof Float32Array &&
      approx(mid[0], 0.5) &&
      approx(mid[1], 0.5) &&
      approx(mid[2], 0) &&
      approx(mid[3], 1),
    `LerpRGBA([1,0,0],[0,1,0],.5) got ${mid && [...mid].join(',')}`,
  );
  /* A follow-up call verifies no exception leaked. */
  const c = RGB(10, 20, 30);
  assert(c instanceof Float32Array, 'RGB after 3-element LerpRGBA still works');
});

safe('HSL / HSLA', () => {
  const h = HSL(0, 1, 0.5);
  assert(
    h instanceof Float32Array && h.length === 4,
    'HSL returns Float32Array(4)',
  );
  const ha = HSLA(0, 1, 0.5, 128);
  assert(
    ha instanceof Float32Array &&
      ha.length === 4 &&
      approx(ha[3], 128 / 255, 1e-3),
    'HSLA has correct alpha',
  );
});

/* ------------------------------------------------------------------ *
 * Group B — plain math helpers                                       *
 * ------------------------------------------------------------------ */
safe('DegToRad / RadToDeg', () => {
  assert(approx(DegToRad(180), Math.PI), 'DegToRad(180) == PI');
  assert(approx(RadToDeg(Math.PI), 180), 'RadToDeg(PI) == 180');
});

/* ------------------------------------------------------------------ *
 * Group C — Transform statics                                        *
 * ------------------------------------------------------------------ */
safe('Transform.Identity', () => {
  const t = Transform.Identity();
  assert(arrApprox([...t], [1, 0, 0, 1, 0, 0]), `Identity got [${[...t]}]`);
});

safe('Transform.Translate static', () => {
  const t = Transform.Translate(10, 20);
  assert(
    arrApprox([...t], [1, 0, 0, 1, 10, 20]),
    `Translate(10,20) got [${[...t]}]`,
  );
});

safe('Transform.Scale static', () => {
  const t = Transform.Scale(3, 4);
  assert(arrApprox([...t], [3, 0, 0, 4, 0, 0]), `Scale(3,4) got [${[...t]}]`);
});

safe('Transform.Rotate static', () => {
  const t = Transform.Rotate(Math.PI / 2);
  /* Rotation 90deg: [cos, sin, -sin, cos, 0, 0] */
  assert(
    approx(t[0], 0) &&
      approx(t[1], 1) &&
      approx(t[2], -1) &&
      approx(t[3], 0) &&
      approx(t[4], 0) &&
      approx(t[5], 0),
    `Rotate(pi/2) got [${[...t]}]`,
  );
});

/* ------------------------------------------------------------------ *
 * Group D — Transform instance methods (nvgjs_transform_function)    *
 * ------------------------------------------------------------------ */
safe('Instance .Translate composes', () => {
  const t = Transform.Identity();
  t.Translate(5, 7);
  assert(
    arrApprox([...t], [1, 0, 0, 1, 5, 7]),
    `identity.Translate(5,7) got [${[...t]}]`,
  );
});

safe('Instance .Scale composes', () => {
  const t = Transform.Identity();
  t.Scale(2, 3);
  assert(
    arrApprox([...t], [2, 0, 0, 3, 0, 0]),
    `identity.Scale(2,3) got [${[...t]}]`,
  );
});

safe('Instance .Rotate composes', () => {
  const t = Transform.Identity();
  t.Rotate(Math.PI);
  /* 180deg on identity: [-1, 0, 0, -1, 0, 0] */
  assert(
    approx(t[0], -1) &&
      approx(t[1], 0, 1e-6) &&
      approx(t[2], 0, 1e-6) &&
      approx(t[3], -1),
    `identity.Rotate(pi) got [${[...t]}]`,
  );
});

/* nanovg's convention (see nvgTransformMultiply in nanovg.c):
     nvgTransformMultiply(t, s)   ==>  t := s * t   ("apply t first, then s")
     nvgTransformPremultiply(t,s) ==>  t := t * s   ("apply s first, then t")
   So instance .Multiply(arg) should compute mat := arg * mat, and
   .Premultiply(arg) should compute mat := mat * arg. */

safe('Instance .Multiply computes mat := arg * mat (nanovg semantics)', () => {
  /* mat=S(2,3), arg=T(5,7). nvgTransformMultiply(S, T) => S := T * S.
     By the formula in nanovg.c:
       new S[4] = S[4]*T[0] + S[5]*T[2] + T[4] = 0 + 0 + 5 = 5
       new S[5] = S[4]*T[1] + S[5]*T[3] + T[5] = 0 + 0 + 7 = 7
     So the translation is *not* scaled — expected [2,0,0,3, 5, 7]. */
  const mat = Transform.Scale(2, 3);
  const arg = Transform.Translate(5, 7);
  mat.Multiply(arg);
  assert(
    arrApprox([...mat], [2, 0, 0, 3, 5, 7], 1e-4),
    `Scale(2,3).Multiply(Translate(5,7)) got [${[...mat]}] expected [2,0,0,3,5,7]`,
  );
});

safe(
  'Instance .Premultiply computes mat := mat * arg (nanovg semantics)',
  () => {
    /* mat=T(5,7), arg=S(2,3). nvgTransformPremultiply(T, S) => T := T * S.
     new T[4] = T[4]*S[0] + T[5]*S[2] + S[4] = 5*2 + 7*0 + 0 = 10
     new T[5] = T[4]*S[1] + T[5]*S[3] + S[5] = 5*0 + 7*3 + 0 = 21   Wait — actually:
     nvgTransformPremultiply(t=T, s=S) sets s2=S; nvgTransformMultiply(s2=S, T) computes
       new S[4] = S[4]*T[0] + S[5]*T[2] + T[4] = 0 + 0 + 5 = 5
       new S[5] = S[4]*T[1] + S[5]*T[3] + T[5] = 0 + 0 + 7 = 7
     then T := s2 = [2,0,0,3, 5, 7]. Expected [2,0,0,3, 5, 7]. */
    const mat = Transform.Translate(5, 7);
    const arg = Transform.Scale(2, 3);
    mat.Premultiply(arg);
    assert(
      arrApprox([...mat], [2, 0, 0, 3, 5, 7], 1e-4),
      `Translate(5,7).Premultiply(Scale(2,3)) got [${[...mat]}] expected [2,0,0,3,5,7]`,
    );
  },
);

safe('.Multiply and .Premultiply disagree (non-commutative sanity)', () => {
  const A = Transform.Scale(2, 3);
  const B = Transform.Scale(2, 3);
  const arg = Transform.Translate(5, 7);
  A.Multiply(arg); // fixed:   A := T * S = [2,0,0,3, 5, 7]
  B.Premultiply(arg); // correct: B := S * T = [2,0,0,3, 10, 21]
  assert(
    !arrApprox([...A], [...B], 1e-6),
    `Multiply and Premultiply must disagree; got A=[${[...A]}] B=[${[...B]}]`,
  );
});

/* ------------------------------------------------------------------ *
 * Group E — TransformPoint free function                             *
 * ------------------------------------------------------------------ */
safe('TransformPoint applies matrix', () => {
  const dst = new Float32Array(2);
  const trf = Transform.Translate(10, 20);
  TransformPoint(dst, trf, 1, 2);
  assert(
    approx(dst[0], 11) && approx(dst[1], 22),
    `TransformPoint((1,2), T(10,20)) got [${[...dst]}]`,
  );
});

/* ------------------------------------------------------------------ *
 * Group F — Transform.TransformPoint instance                        *
 * ------------------------------------------------------------------ */
safe('Instance .TransformPoint', () => {
  const t = Transform.Translate(3, 4);
  const p = t.TransformPoint(1, 1);
  assert(
    p && approx(p[0], 4) && approx(p[1], 5),
    `T(3,4).TransformPoint(1,1) got [${p}]`,
  );
});

/* ------------------------------------------------------------------ *
 * Group G — second-round fixes                                       *
 * ------------------------------------------------------------------ */

safe(
  'Transform.Multiply single-arg does not corrupt input (uninit tmp fix)',
  () => {
    /* Before the fix, Transform.Multiply(oneArg) fell through with mat=tmp
     uninitialised, then wrote garbage back into argv[0]. After the fix tmp
     is seeded with identity, so Transform.Multiply(a) leaves `a` as a * I = a. */
    const a = new Float32Array([2, 0, 0, 3, 5, 7]);
    Transform.Multiply(a);
    assert(
      arrApprox([...a], [2, 0, 0, 3, 5, 7], 1e-4),
      `Transform.Multiply(a) should leave a untouched; got [${[...a]}]`,
    );
  },
);

safe('nvgjs_transform_copy no longer pollutes Arrays with named props', () => {
  /* nvgjs_copy used to run copyarray AND copyobject unconditionally. So
     Transform.Identity(arr) (via CurrentTransform-style back-copy) left the
     array with named "a".."f" props on top of its indexed slots. */
  const arr = [1, 2, 3, 4, 5, 6];
  Transform.Identity(arr);
  assert(
    arrApprox(arr, [1, 0, 0, 1, 0, 0], 1e-4),
    `Identity(arr) indexed slots wrong: [${arr}]`,
  );
  assert(
    !('a' in arr) && !('b' in arr) && !('e' in arr),
    `Identity(arr) leaked named props a/b/e onto plain Array: keys=${Object.keys(arr)}`,
  );
});

/* ------------------------------------------------------------------ */
console.log(`\nRESULTS: ${passed} passed, ${failed} failed`);
if(failed > 0) {
  console.log('Failures:');
  for(const f of failures) console.log('  -', f);
  throw new Error(`${failed} test(s) failed`);
}
