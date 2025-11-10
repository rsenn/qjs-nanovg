/**
 * @file js-utils.h
 */
#ifndef JS_UTILS_H
#define JS_UTILS_H

#include <quickjs.h>
#include <cutils.h>

int nvgjs_inputoutputarray(JSContext*, float*, size_t, JSValueConst);
int nvgjs_inputobject(JSContext*, float*, size_t, const char* const[], JSValueConst);
int nvgjs_inputarray(JSContext*, float*, size_t, JSValueConst);
int nvgjs_inputiterator(JSContext*, float*, size_t, JSValueConst);
int nvgjs_input(JSContext*, float*, size_t, const char* const[], JSValueConst);
float* nvgjs_outputarray(JSContext*, size_t*, JSValueConst);
int nvgjs_copyobject(JSContext*, JSValueConst, const char* const[], const float*, size_t);
int nvgjs_copyarray(JSContext*, JSValueConst, const float*, size_t);

static inline int
nvgjs_tofloat32(JSContext* ctx, float* pres, JSValueConst val) {
  double f;
  int ret;

  if(!(ret = JS_ToFloat64(ctx, &f, val)))
    *pres = (float)f;

  return ret;
}

static inline BOOL
nvgjs_same_object(JSValueConst a, JSValueConst b) {
  return JS_IsObject(a) && JS_IsObject(b) && JS_VALUE_GET_PTR(a) == JS_VALUE_GET_PTR(b);
}

#endif /* defined JS_UTILS_H */
