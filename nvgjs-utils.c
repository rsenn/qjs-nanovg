#include "nvgjs-utils.h"
#include <assert.h>
#include <string.h>

static int
nvgjs_arraylen(JSContext* ctx, JSValueConst vector) {
  int32_t len = -1;
  JSValue val = JS_GetPropertyStr(ctx, vector, "length");
  JS_ToInt32(ctx, &len, val);
  JS_FreeValue(ctx, val);
  return len;
}

static void*
nvgjs_typedarray(JSContext* ctx, JSValueConst obj, int* plength, int* pbytes_per_element) {
  size_t offset, byte_length, bytes_per_element;
  uint8_t* ptr = 0;
  JSValue buf = JS_GetTypedArrayBuffer(ctx, obj, &offset, &byte_length, &bytes_per_element);

  if(!JS_IsException(buf)) {
    assert(bytes_per_element);

    *plength = byte_length / bytes_per_element;
    *pbytes_per_element = bytes_per_element;

    if((ptr = JS_GetArrayBuffer(ctx, &byte_length, buf)))
      ptr += offset;

    JS_FreeValue(ctx, buf);
  } else {
    JS_GetException(ctx);
  }

  return ptr;
}

static JSAtom
nvgjs_iterator(JSContext* ctx) {
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

static JSValue
nvgjs_next(JSContext* ctx, JSValueConst obj, BOOL* done_p) {
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
nvgjs_outputarray(JSContext* ctx, int* plength, JSValueConst value) {
  int bytes_per_element = 0;
  float* ptr;

  if((ptr = nvgjs_typedarray(ctx, value, plength, &bytes_per_element)))
    if(bytes_per_element == sizeof(float))
      return ptr;

  JS_ThrowTypeError(ctx, "expecting a Float32Array");
  return 0;
}

float*
nvgjs_output(JSContext* ctx, int min_length, JSValueConst value) {
  int len;
  float* ptr;

  if((ptr = nvgjs_outputarray(ctx, &len, value)))
    if(len >= min_length)
      return ptr;

  return 0;
}

float*
nvgjs_inputoutputarray(JSContext* ctx, float vec[], int min_length, JSValueConst vector) {
  float* ptr;
  int len;

  if((ptr = nvgjs_outputarray(ctx, &len, vector))) {
    if(len < min_length) {
      JS_ThrowRangeError(ctx, "TypedArray vector must have at least %i elements (has %i)", min_length, len);
      return 0;
    }

    return ptr;
  }

  if(!nvgjs_inputarray(ctx, vec, min_length, vector))
    return vec;

  if(JS_HasProperty(ctx, vector, nvgjs_iterator(ctx)))
    if(!nvgjs_inputiterator(ctx, vec, min_length, vector))
      return vec;

  JS_ThrowTypeError(ctx, "Expecting an Float32Array, Array or Iterable");
  return 0;
}

int
nvgjs_inputobject(JSContext* ctx, float vec[], int len, const char* const prop_map[], JSValueConst vector) {
  for(int i = 0; i < len; i++) {
    JSValue value = JS_GetPropertyStr(ctx, vector, prop_map[i]);

    if(nvgjs_tofloat32(ctx, &vec[i], value))
      return -1;

    JS_FreeValue(ctx, value);
  }

  return 0;
}

int
nvgjs_inputarray(JSContext* ctx, float vec[], int min_length, JSValueConst vector) {
  float* ptr;
  int length;

  if((ptr = nvgjs_outputarray(ctx, &length, vector))) {
    if(length < min_length) {
      JS_ThrowRangeError(ctx, "TypedArray vector must have at least %i elements (has %i)", min_length, length);
      return -1;
    }

    memcpy(vec, ptr, min_length * sizeof(float));
    return 0;
  }

  if(nvgjs_arraylen(ctx, vector) < min_length) {
    JS_ThrowRangeError(ctx, "input Array must have at least %i elements", min_length);
    return -1;
  }

  for(int i = 0; i < min_length; i++) {
    JSValue value = JS_GetPropertyUint32(ctx, vector, i);

    if(nvgjs_tofloat32(ctx, &vec[i], value))
      return -1;

    JS_FreeValue(ctx, value);
  }

  return 0;
}

int
nvgjs_inputiterator(JSContext* ctx, float vec[], int min_length, JSValueConst vector) {
  JSValue iter = JS_Invoke(ctx, vector, nvgjs_iterator(ctx), 0, 0);

  if(JS_IsException(iter)) {
    JS_ThrowTypeError(ctx, "object must be iterable");
    return -1;
  }

  for(int i = 0; i < min_length; i++) {
    BOOL done = FALSE;
    JSValue val = nvgjs_next(ctx, iter, &done);
    int ret = 0;

    if(!done)
      ret = nvgjs_tofloat32(ctx, &vec[i], val);

    JS_FreeValue(ctx, val);

    if(ret)
      return -1;

    if(done) {
      JS_ThrowRangeError(ctx, "iterable must have at least %i elements (has %i)", min_length, i);
      return -1;
    }
  }

  JS_FreeValue(ctx, iter);
  return 0;
}

int
nvgjs_input(JSContext* ctx, float vec[], int len, const char* const prop_map[], JSValueConst vector) {
  if(!JS_IsObject(vector)) {
    JS_ThrowTypeError(ctx, "vector must be an object");
    return -1;
  }

  if(!nvgjs_inputarray(ctx, vec, len, vector))
    return 0;

  if(JS_HasProperty(ctx, vector, nvgjs_iterator(ctx)))
    if(!nvgjs_inputiterator(ctx, vec, len, vector))
      return 0;

  if(prop_map)
    if(!nvgjs_inputobject(ctx, vec, len, prop_map, vector))
      return 0;

  JS_ThrowTypeError(ctx, "object must be iterable or a property Map must be supplied");
  return -1;
}

void
nvgjs_copyobject(JSContext* ctx, JSValueConst value, const char* const prop_map[], const float vec[], int len) {

  for(int i = 0; i < len; i++)
    JS_SetPropertyStr(ctx, value, prop_map[i], JS_NewFloat64(ctx, vec[i]));
}

void
nvgjs_copyarray(JSContext* ctx, JSValueConst value, const float vec[], int len) {
  for(int i = 0; i < len; i++)
    JS_SetPropertyUint32(ctx, value, i, JS_NewFloat64(ctx, vec[i]));
}
