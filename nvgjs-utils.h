/**
 * @file js-utils.h
 */
#ifndef JS_UTILS_H
#define JS_UTILS_H

#include <quickjs.h>
#include <cutils.h>

/**
 * @brief Get a writable pointer into a Float32Array's backing store
 * (zero-copy).
 *
 * Used when a NanoVG function needs to write results directly into a JS-owned
 * typed array. Throws a TypeError on the QuickJS context if @p value is not a
 * Float32Array.
 *
 * @param      ctx      QuickJS context (for exceptions).
 * @param[out] plength  Receives the array length in elements (floats).
 * @param      value    The JS value, expected to be a Float32Array.
 * @return Pointer to the float data, or NULL on type mismatch.
 */
float* nvgjs_outputarray(JSContext*, int* plength, JSValueConst);

/**
 * @brief Like nvgjs_outputarray(), but require a minimum capacity.
 *
 * Convenience wrapper used as an output buffer for functions that produce a
 * fixed number of floats; only returns the pointer if the array is large
 * enough.
 *
 * @param ctx         QuickJS context (for exceptions).
 * @param min_length  Minimum number of float elements required.
 * @param value       The JS value, expected to be a Float32Array.
 * @return Pointer to the float data, or NULL if not a Float32Array or too
 * short.
 */
float* nvgjs_output(JSContext*, int min_length, JSValueConst);

/**
 * @brief Obtain a float buffer for an in/out vector argument, flexibly.
 *
 * If @p vector is a Float32Array it returns a direct (zero-copy) pointer into
 * its storage; otherwise it reads the values out of a plain Array or any
 * iterable into the caller-supplied scratch buffer @p vec and returns that.
 * Used for arguments NanoVG both reads and may write back.
 *
 * @param      ctx         QuickJS context (for exceptions).
 * @param[out] vec         Caller-provided fallback buffer (used for non-typed
 * inputs).
 * @param      min_length  Minimum number of elements required.
 * @param      vector      The JS value: Float32Array, Array, or iterable.
 * @return Pointer to usable float data (either @p vector's buffer or @p vec),
 * or NULL on error.
 */
float* nvgjs_inputoutputarray(JSContext*, float[], int min_length, JSValueConst);

/**
 * @brief Read floats from named properties of a JS object into a C array.
 *
 * For each i in [0,len) reads @p vector[prop_map[i]] and converts it to a
 * float. Used to accept vector arguments given as objects, e.g. {x, y} or
 * {r,g,b,a}.
 *
 * @param      ctx       QuickJS context (for exceptions).
 * @param[out] vec       Destination buffer of at least @p len floats.
 * @param      len       Number of properties/floats to read.
 * @param      prop_map  Array of @p len property-name strings to read from.
 * @param      vector    The source JS object.
 * @return 0 on success, -1 on conversion error.
 */
int nvgjs_inputobject(JSContext*, float[], int len, const char* const prop_map[], JSValueConst);

/**
 * @brief Read floats from a Float32Array or plain Array into a C array.
 *
 * Fast path memcpy's from a Float32Array; otherwise reads indexed elements of a
 * plain Array. Throws a RangeError if fewer than @p min_length elements exist.
 *
 * @param      ctx         QuickJS context (for exceptions).
 * @param[out] vec         Destination buffer of at least @p min_length floats.
 * @param      min_length  Number of elements to read.
 * @param      vector      The source JS value: Float32Array or Array.
 * @return 0 on success, -1 on type/range/conversion error.
 */
int nvgjs_inputarray(JSContext*, float[], int min_length, JSValueConst);

/**
 * @brief Read floats from any iterable (Symbol.iterator) into a C array.
 *
 * Pulls @p min_length values via the iterator protocol. Throws if the object is
 * not iterable or is exhausted early.
 *
 * @param      ctx         QuickJS context (for exceptions).
 * @param[out] vec         Destination buffer of at least @p min_length floats.
 * @param      min_length  Number of elements to read.
 * @param      vector      The source iterable JS value.
 * @return 0 on success, -1 on error.
 */
int nvgjs_inputiterator(JSContext*, float[], int min_length, JSValueConst);

/**
 * @brief General-purpose vector argument reader.
 *
 * Tries, in order: Array/Float32Array, then iterable, then (if @p prop_map is
 * given) named object properties. This is the catch-all entry point for
 * accepting a vector argument in whatever JS form the caller provides.
 *
 * @param      ctx       QuickJS context (for exceptions).
 * @param[out] vec       Destination buffer of at least @p len floats.
 * @param      len       Number of elements to read.
 * @param      prop_map  Property-name strings for the object fallback, or NULL.
 * @param      vector    The source JS value (must be an object).
 * @return 0 on success, -1 if @p vector is not an object or no form matched.
 */
int nvgjs_input(JSContext*, float[], int len, const char* const prop_map[], JSValueConst);

/**
 * @brief Write a C float array back into named properties of a JS object.
 *
 * Inverse of nvgjs_inputobject(): sets @p value[prop_map[i]] = vec[i]. Used to
 * return results (e.g. text bounds) as a JS object with named fields.
 *
 * @param ctx       QuickJS context.
 * @param value     Destination JS object.
 * @param prop_map  Array of @p len property-name strings to assign.
 * @param vec       Source float values.
 * @param len       Number of values to write.
 */
void nvgjs_copyobject(JSContext*, JSValueConst, const char* const prop_map[], const float[], int len);

/**
 * @brief Write a C float array into the indexed elements of a JS array/object.
 *
 * Sets @p value[i] = vec[i] for i in [0,len). Used to return results as an
 * Array-like JS value.
 *
 * @param ctx    QuickJS context.
 * @param value  Destination JS array/object.
 * @param vec    Source float values.
 * @param len    Number of values to write.
 */
void nvgjs_copyarray(JSContext*, JSValueConst, const float[], int len);

/**
 * @brief Accept a vector either as one object argument or as separate scalars.
 *
 * If the first argument is an object it is read as a vector (via
 * nvgjs_inputarray); otherwise @p vlen scalar arguments are converted. Handles
 * the common overload "fn(x, y)" vs "fn([x, y])".
 *
 * @param      ctx   QuickJS context (for exceptions).
 * @param[out] vec   Destination buffer (sized for the vector, e.g. 2 floats).
 * @param      vlen  Number of floats the vector holds.
 * @param      argc  Argument count as passed to the JS function.
 * @param      argv  Argument values as passed to the JS function.
 * @return Number of elements filled (the vector length), or 0 if none matched.
 */
int nvgjs_arguments(JSContext*, float[], int, int, JSValueConst[]);

/**
 * @brief Convert a JS value to a 32-bit float.
 *
 * Thin wrapper over JS_ToFloat64() that narrows the result to float. Used
 * pervasively when reading numeric arguments for NanoVG (which takes floats).
 *
 * @param      ctx   QuickJS context (for exceptions).
 * @param[out] pres  Receives the converted float (only written on success).
 * @param      val   The JS value to convert.
 * @return 0 on success, non-zero if conversion failed (exception pending).
 */
static inline int
nvgjs_tofloat32(JSContext* ctx, float* pres, JSValueConst val) {
  double f;
  int ret;

  if(!(ret = JS_ToFloat64(ctx, &f, val)))
    *pres = (float)f;

  return ret;
}

/**
 * @brief Convert a Unicode character index into a UTF-8 byte offset.
 *
 * Walks @p charpos code points from the start of @p str and returns the byte
 * offset reached. Used to map JS string character positions onto the byte
 * offsets NanoVG's text functions expect.
 *
 * @param str      Pointer to the UTF-8 string data.
 * @param len      Length of @p str in bytes.
 * @param charpos  Target character (code point) index.
 * @return Byte offset of the @p charpos-th character (clamped to @p len).
 */
static size_t
nvgjs_utf8offset(const void* str, size_t len, int charpos) {
  const uint8_t *p = (const uint8_t*)str, *e = p + len;

  for(int i = 0; i < charpos && p < e; i++)
    if(unicode_from_utf8(p, e - p, &p) == -1)
      break;

  return p - (const uint8_t*)str;
}

/**
 * @brief Test whether two JS values reference the same object.
 *
 * Both must be objects and share the same underlying pointer (identity, not
 * structural equality). Used to detect aliasing between arguments.
 *
 * @param a First JS value.
 * @param b Second JS value.
 * @return TRUE if both are objects with identical pointers, FALSE otherwise.
 */
static inline BOOL
nvgjs_same_object(JSValueConst a, JSValueConst b) {
  return JS_IsObject(a) && JS_IsObject(b) && JS_VALUE_GET_PTR(a) == JS_VALUE_GET_PTR(b);
}

#endif /* defined JS_UTILS_H */
