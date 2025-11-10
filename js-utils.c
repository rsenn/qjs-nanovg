#include "js-utils.h"
#include <string.h>

static JSAtom iterator_symbol;

static JSAtom
nvgjs_iterator_symbol(JSContext* ctx) {
  static JSAtom iterator;

  if(!iterator) {
    JSValue ret = JS_UNDEFINED;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_GetPropertyStr(ctx, global, "Symbol");
    JS_FreeValue(ctx, global);
    JSValue sym = JS_GetPropertyStr(ctx, ctor, "iterator");
    JS_FreeValue(ctx, ctor);
    JSAtom iterator = JS_ValueToAtom(ctx, sym);
    JS_FreeValue(ctx, sym);
  }

  return iterator;
}

JSValue
nvgjs_iterator_invoke(JSContext* ctx, JSValueConst obj) {
  JSAtom prop = nvgjs_iterator_symbol(ctx);
  return JS_Invoke(ctx, obj, prop, 0, 0);
}

void*
nvgjs_typedarray(JSContext* ctx, JSValueConst obj, size_t* plength, size_t* pbytes_per_element) {
  size_t offset = 0, len;
  uint8_t* ptr;
  JSValue buf = JS_GetTypedArrayBuffer(ctx, obj, &offset, plength, pbytes_per_element);

  if((ptr = JS_GetArrayBuffer(ctx, &len, buf)))
    ptr += offset;

  JS_FreeValue(ctx, buf);
  return ptr;
}

JSValue
nvgjs_iterator_next(JSContext* ctx, JSValueConst obj, BOOL* done_p) {
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

float*
nvgjs_outputarray(JSContext* ctx, size_t* plength, JSValueConst value) {
  size_t bytes_per_element = 0;
  float* ptr;

  if((ptr = nvgjs_typedarray(ctx, value, plength, &bytes_per_element)))
    if(bytes_per_element == sizeof(float))
      return ptr;

  return 0;
}

int
nvgjs_inputoutputarray(JSContext* ctx, float* vec, size_t len, JSValueConst value) {
  float* ptr;
  size_t length;

  if((ptr = nvgjs_outputarray(ctx, &length, value))) {
    if(length < len) {
      JS_ThrowTypeError(ctx, "TypedArray value must have at least %zu elements (has %zu)", len, length);
      return -1;
    }

    memcpy(vec, ptr, len * sizeof(float));
    return 0;
  }

  return nvgjs_inputarray(ctx, vec, len, value);
}

int
nvgjs_inputobject(JSContext* ctx, float* vec, size_t len, const char* const prop_map[], JSValueConst vector) {
  for(int i = 0; i < len; i++) {
    JSValue value = JS_GetPropertyStr(ctx, vector, prop_map[i]);
    if(js_tofloat32(ctx, &vec[i], value))
      return -1;
    JS_FreeValue(ctx, value);
  }

  return 0;
}

int
nvgjs_inputarray(JSContext* ctx, float* vec, size_t len, JSValueConst vector) {
  for(int i = 0; i < len; i++) {
    JSValue value = JS_GetPropertyUint32(ctx, vector, i);
    if(js_tofloat32(ctx, &vec[i], value))
      return -1;
    JS_FreeValue(ctx, value);
  }

  return 0;
}

int
nvgjs_inputiterator(JSContext* ctx, float* vec, size_t len, JSValueConst vector) {
  JSValue iter = nvgjs_iterator_invoke(ctx, vector);

  if(JS_IsObject(iter)) {
    for(int i = 0; i < len; i++) {
      BOOL done = FALSE;
      JSValue val = nvgjs_iterator_next(ctx, iter, &done);
      int ret = 0;

      if(!done)
        ret = js_tofloat32(ctx, &vec[i], val);

      JS_FreeValue(ctx, val);

      if(ret)
        return -1;

      if(done) {
        JS_ThrowRangeError(ctx, "iterable must have at least %zu elements (has %d)", len, i);
        return -1;
      }
    }

    JS_FreeValue(ctx, iter);
    return 0;
  }

  JS_ThrowTypeError(ctx, "object must be iterable");
  return -1;
}

int
nvgjs_input(JSContext* ctx, float* vec, size_t len, const char* const prop_map[], JSValueConst vector) {
  if(!JS_IsObject(vector)) {
    JS_ThrowTypeError(ctx, "vector must be an object");
    return -1;
  }

  if(JS_IsArray(ctx, vector))
    return nvgjs_inputarray(ctx, vec, len, vector);

  if(JS_HasProperty(ctx, vector, nvgjs_iterator_symbol(ctx)))
    return nvgjs_inputiterator(ctx, vec, len, vector);

  if(prop_map)
    return nvgjs_inputobject(ctx, vec, len, prop_map, vector);

  JS_ThrowTypeError(ctx, "object must be iterable or a property Map must be supplied");
  return -1;
}

int
js_fromfloat32v(JSContext* ctx, const float* vec, size_t len, const char* const prop_map[], JSValueConst value) {
  size_t length;

  if(nvgjs_outputarray(ctx, &length, value) && length >= len)
    return 0;

  return js_float32v_store(ctx, vec, len, prop_map, value);
}

int
js_float32v_store(JSContext* ctx, const float* vec, size_t len, const char* const prop_map[], JSValueConst value) {
  size_t length;

  if(!JS_IsObject(value)) {
    JS_ThrowTypeError(ctx, "value must be an object");
    return -1;
  }

  if(JS_IsArray(ctx, value)) {
    for(int i = 0; i < len; i++)
      JS_SetPropertyUint32(ctx, value, i, JS_NewFloat64(ctx, vec[i]));

    return 1;
  }

  if(prop_map) {
    for(int i = 0; i < len; i++)
      JS_SetPropertyStr(ctx, value, prop_map[i], JS_NewFloat64(ctx, vec[i]));

    return 1;
  }

  return 0;
}
