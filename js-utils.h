/**
 * @file js-utils.h
 */
#ifndef JS_UTILS_H
#define JS_UTILS_H

#include <quickjs.h>

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

#endif /* defined JS_UTILS_H */
