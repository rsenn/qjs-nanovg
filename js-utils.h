/**
 * @file js-utils.h
 */
#ifndef JS_UTILS_H
#define JS_UTILS_H

#include <quickjs.h>
#include <cutils.h>

static inline int
js_tofloat32(JSContext* ctx, float* pres, JSValueConst val) {
  double f;
  int ret;

  if(!(ret = JS_ToFloat64(ctx, &f, val)))
    *pres = (float)f;

  return ret;
}

static inline int
js_get_property_str_float32(JSContext* ctx, JSValueConst this_obj, const char* prop, float* pres) {
  JSValue p = JS_GetPropertyStr(ctx, this_obj, prop);
  int ret = js_tofloat32(ctx, pres, p);
  JS_FreeValue(ctx, p);
  return ret;
}

static inline int
js_get_property_uint_float32(JSContext* ctx, JSValueConst this_obj, uint32_t idx, float* pres) {
  JSValue p = JS_GetPropertyUint32(ctx, this_obj, idx);
  int ret = js_tofloat32(ctx, pres, p);
  JS_FreeValue(ctx, p);
  return ret;
}

static inline JSValue
js_iterator_method(JSContext* ctx, JSValueConst obj) {
  JSValue ret = JS_UNDEFINED;
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue ctor = JS_GetPropertyStr(ctx, global, "Symbol");
  JS_FreeValue(ctx, global);
  JSValue sym = JS_GetPropertyStr(ctx, ctor, "iterator");
  JS_FreeValue(ctx, ctor);
  JSAtom atom = JS_ValueToAtom(ctx, sym);
  JS_FreeValue(ctx, sym);

  if(JS_HasProperty(ctx, obj, atom))
    ret = JS_GetProperty(ctx, obj, atom);

  JS_FreeAtom(ctx, atom);
  return ret;
}

static inline JSValue
js_iterator_new(JSContext* ctx, JSValueConst obj) {
  JSValue ret = JS_UNDEFINED, fn = js_iterator_method(ctx, obj);

  if(JS_IsFunction(ctx, fn))
    ret = JS_Call(ctx, fn, obj, 0, 0);

  JS_FreeValue(ctx, fn);
  return ret;
}

static inline JSValue
js_iterator_next(JSContext* ctx, JSValueConst obj, BOOL* done_p) {
  JSValue fn = JS_GetPropertyStr(ctx, obj, "next");
  JSValue result = JS_Call(ctx, fn, obj, 0, 0);
  JS_FreeValue(ctx, fn);
  JSValue done = JS_GetPropertyStr(ctx, result, "done");
  JSValue value = JS_GetPropertyStr(ctx, result, "value");
  JS_FreeValue(ctx, result);
  *done_p = JS_ToBool(ctx, done);
  JS_FreeValue(ctx, done);
  return value;
}

#endif /* defined JS_UTILS_H */
