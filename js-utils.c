#include "js-utils.h"
#include <string.h>

int
js_get_property_str_float32(JSContext* ctx, JSValueConst this_obj, const char* prop, float* pres) {
  JSValue p = JS_GetPropertyStr(ctx, this_obj, prop);
  int ret = js_tofloat32(ctx, pres, p);
  JS_FreeValue(ctx, p);
  return ret;
}

int
js_get_property_uint_float32(JSContext* ctx, JSValueConst this_obj, uint32_t idx, float* pres) {
  JSValue p = JS_GetPropertyUint32(ctx, this_obj, idx);
  int ret = js_tofloat32(ctx, pres, p);
  JS_FreeValue(ctx, p);
  return ret;
}

void*
js_get_typedarray(JSContext* ctx, JSValueConst obj, size_t* plength, size_t* pbytes_per_element) {
  size_t offset = 0, len;
  uint8_t* ptr;
  JSValue buf = JS_GetTypedArrayBuffer(ctx, obj, &offset, plength, pbytes_per_element);

  if((ptr = JS_GetArrayBuffer(ctx, &len, buf)))
    ptr += offset;

  JS_FreeValue(ctx, buf);
  return ptr;
}

JSValue
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

JSValue
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

float*
js_float32v_ptr(JSContext* ctx, size_t* plength, JSValueConst value) {
  size_t bytes_per_element = 0;
  float* ptr;

  if((ptr = js_get_typedarray(ctx, value, plength, &bytes_per_element))) {
    /*if(length < min_len) {
      JS_ThrowTypeError(ctx, "TypedArray value must have at least %zu elements (has %zu)", min_len, length);
      return 0;
    }*/

    if(bytes_per_element == sizeof(float))
      return ptr;
  }

  return 0;
}

int
js_tofloat32v(JSContext* ctx, float* vec, size_t min_len, const char* const prop_map[], JSValueConst value) {
  float* ptr;
  size_t length;

  if((ptr = js_float32v_ptr(ctx, &length, value))) {
    if(length < min_len) {
      JS_ThrowTypeError(ctx, "TypedArray value must have at least %zu elements (has %zu)", min_len, length);
      return -1;
    }

    memcpy(vec, ptr, min_len * sizeof(float));
    return 0;
  }

  return js_float32v_load(ctx, vec, min_len, prop_map, value);
}

int
js_float32v_load(JSContext* ctx, float* vec, size_t min_len, const char* const prop_map[], JSValueConst value) {
  if(!JS_IsObject(value)) {
    JS_ThrowTypeError(ctx, "value must be an object");
    return -1;
  }

  if(JS_IsArray(ctx, value)) {
    for(int i = 0; i < min_len; i++)
      if(js_get_property_uint_float32(ctx, value, i, &vec[i]))
        return -1;

    return 0;
  }

  JSValue iter = js_iterator_new(ctx, value);

  if(JS_IsObject(iter)) {
    for(int i = 0; i < min_len; i++) {
      BOOL done = FALSE;
      JSValue val = js_iterator_next(ctx, iter, &done);
      int ret = 0;

      if(!done)
        ret = js_tofloat32(ctx, &vec[i], val);

      JS_FreeValue(ctx, val);

      if(ret)
        return -1;

      if(done) {
        JS_ThrowRangeError(ctx, "iterable must have at least %zu elements (has %d)", min_len, i);
        return -1;
      }
    }

    JS_FreeValue(ctx, iter);
    return 0;
  }

  if(prop_map) {
    for(int i = 0; i < min_len; i++)
      if(js_get_property_str_float32(ctx, value, prop_map[i], &vec[i]))
        return -1;

    return 0;
  }

  JS_ThrowTypeError(ctx, "value must be iterable");
  return -1;
}

int
js_fromfloat32v(JSContext* ctx, const float* vec, size_t min_len, const char* const prop_map[], JSValueConst value) {
  size_t length;

  if(js_float32v_ptr(ctx, &length, value) && length >= min_len)
    return 0;

  return js_float32v_store(ctx, vec, min_len, prop_map, value);
}

int
js_float32v_store(JSContext* ctx, const float* vec, size_t min_len, const char* const prop_map[], JSValueConst value) {
  size_t length;

  if(!JS_IsObject(value)) {
    JS_ThrowTypeError(ctx, "value must be an object");
    return -1;
  }

  if(JS_IsArray(ctx, value)) {
    for(int i = 0; i < min_len; i++)
      JS_SetPropertyUint32(ctx, value, i, JS_NewFloat64(ctx, vec[i]));

    return 1;
  }

  if(prop_map) {
    for(int i = 0; i < min_len; i++)
      JS_SetPropertyStr(ctx, value, prop_map[i], JS_NewFloat64(ctx, vec[i]));

    return 1;
  }

  return 0;
}
