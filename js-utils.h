/**
 * @file js-utils.h
 */
#ifndef JS_UTILS_H
#define JS_UTILS_H

#include <quickjs.h>
#include <cutils.h>

JSValue nvgjs_iterator_invoke(JSContext*, JSValueConst);
void* nvgjs_typedarray(JSContext*, JSValueConst, size_t*, size_t*);
JSValue nvgjs_iterator_next(JSContext*, JSValueConst, BOOL*);
float* nvgjs_outputarray(JSContext*, size_t*, JSValueConst);
int nvgjs_inputoutputarray(JSContext*, float*, size_t, JSValueConst);
int nvgjs_inputobject(JSContext*, float*, size_t, const char* const[], JSValueConst);
int nvgjs_inputarray(JSContext*, float*, size_t, JSValueConst);
int nvgjs_inputiterator(JSContext*, float*, size_t, JSValueConst);
int nvgjs_input(JSContext*, float*, size_t, const char* const[], JSValueConst);
int js_fromfloat32v(JSContext*, const float*, size_t, const char* const[], JSValueConst);
int js_float32v_store(JSContext*, const float*, size_t, const char* const[], JSValueConst);

static inline int
js_tofloat32(JSContext* ctx, float* pres, JSValueConst val) {
  double f;
  int ret;

  if(!(ret = JS_ToFloat64(ctx, &f, val)))
    *pres = (float)f;

  return ret;
}

static inline BOOL
js_is_same_obj(JSValueConst a, JSValueConst b) {
  return JS_IsObject(a) && JS_IsObject(b) && JS_VALUE_GET_PTR(a) == JS_VALUE_GET_PTR(b);
}

#endif /* defined JS_UTILS_H */
