#ifndef NANOVG_QJS_H
#define NANOVG_QJS_H

#include <quickjs.h>

#ifdef JS_SHARED_LIBRARY
#if defined(_WIN32) || defined(__MINGW32__)
#define VISIBLE __declspec(dllexport)
#else
#define VISIBLE __attribute__((visibility("default")))
#endif
#define nvgjs_init_module js_init_module
#else
#define VISIBLE
#endif

#define NVGJS_DECL(class, fn) static JSValue nvgjs_##class##_##fn(JSContext* ctx, JSValueConst this_obj, int argc, JSValueConst argv[], int magic)

#define NVGJS_CONTEXT(this_obj) \
  NVGcontext* nvg; \
  if(!(nvg = JS_GetOpaque2(ctx, this_obj, nvgjs_context_class_id))) \
    return JS_EXCEPTION;

#define NVGJS_FUNC(fn, length) JS_CFUNC_MAGIC_DEF(#fn, length, nvgjs_func_##fn, 0)
#define NVGJS_METHOD(class, fn, length) JS_CFUNC_MAGIC_DEF(#fn, length, nvgjs_##class##_##fn, 0)
#define NVGJS_CFUNC_DEC(name, fn, length) JS_CFUNC_MAGIC_DEF(name, length, nvgjs_func_##fn, 1)
#define NVGJS_FLAG(name) JS_PROP_INT32_DEF(#name, NVG_##name, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE)

VISIBLE JSModuleDef* js_init_module_nanovg(JSContext* ctx, const char* module_name);

#endif /* NANOVG_QJS_H */
