/**
 * @file js-utils.h
 */
#ifndef JS_UTILS_H
#define JS_UTILS_H

#include <quickjs.h>
#include <cutils.h>

float* nvgjs_outputarray(JSContext*, int* plength, JSValueConst value);
float* nvgjs_output(JSContext*, int min_length, JSValueConst value);
float* nvgjs_inputoutputarray(JSContext*, float vec[], int min_length, JSValueConst);
int nvgjs_inputobject(JSContext*, float vec[], int len, const char* const prop_map[], JSValueConst);
int nvgjs_inputarray(JSContext*, float vec[], int min_length, JSValueConst);
int nvgjs_inputiterator(JSContext*, float vec[], int min_length, JSValueConst);
int nvgjs_input(JSContext*, float vec[], int len, const char* const prop_map[], JSValueConst);
void nvgjs_copyobject(JSContext*, JSValueConst value, const char* const prop_map[], const float vec[], int len);
void nvgjs_copyarray(JSContext*, JSValueConst value, const float vec[], int len);

static inline int
nvgjs_tofloat32(JSContext* ctx, float* pres, JSValueConst val) {
  double f;
  int ret;

  if(!(ret = JS_ToFloat64(ctx, &f, val)))
    *pres = (float)f;

  return ret;
}

static size_t
nvgjs_utf8offset(const void* str, size_t len, int charpos) {
  const uint8_t *p = (const uint8_t*)str, *e = p + len;

  for(int i = 0; i < charpos && p < e; i++)
    if(unicode_from_utf8(p, e - p, &p) == -1)
      break;

  return p - (const uint8_t*)str;
}

static inline BOOL
nvgjs_same_object(JSValueConst a, JSValueConst b) {
  return JS_IsObject(a) && JS_IsObject(b) && JS_VALUE_GET_PTR(a) == JS_VALUE_GET_PTR(b);
}

#endif /* defined JS_UTILS_H */
