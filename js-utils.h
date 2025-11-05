/**
 * @file js-utils.h
 */
#ifndef JS_UTILS_H
#define JS_UTILS_H

#include <quickjs.h>
#include <cutils.h>

int js_get_property_str_float32(JSContext*, JSValueConst, const char*, float*);
int js_get_property_uint_float32(JSContext*, JSValueConst, uint32_t, float*);
void* js_get_typedarray(JSContext*, JSValueConst, size_t*, size_t*);
JSValue js_iterator_method(JSContext*, JSValueConst);
JSValue js_iterator_next(JSContext*, JSValueConst, BOOL*);
float* js_float32v_ptr(JSContext*, size_t, JSValueConst);
int js_tofloat32v(JSContext*, float*, size_t, const char* const[], JSValueConst);
int js_float32v_copy(JSContext*, const float*, size_t, const char* const[], JSValueConst);

static inline int
js_tofloat32(JSContext* ctx, float* pres, JSValueConst val) {
  double f;
  int ret;

  if(!(ret = JS_ToFloat64(ctx, &f, val)))
    *pres = (float)f;

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

#endif /* defined JS_UTILS_H */
