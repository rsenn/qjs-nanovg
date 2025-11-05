#ifndef NANOVG_QJS_H
#define NANOVG_QJS_H

#include <quickjs.h>

#ifdef JS_SHARED_LIBRARY
#if defined(_WIN32) || defined(__MINGW32__)
#define VISIBLE __declspec(dllexport)
#else
#define VISIBLE __attribute__((visibility("default")))
#endif
#define js_init_module_nanovg js_init_module
#else
#define VISIBLE
#endif

#define NVGJS_DECL(fn) static JSValue nvgjs_##fn(JSContext* ctx, JSValueConst this_value, int argc, JSValueConst argv[])
#define NVGJS_FUNC(fn, length) JS_CFUNC_DEF(#fn, length, nvgjs_##fn)
#define NVGJS_FLAG(name) JS_PROP_INT32_DEF(#name, NVG_##name, JS_PROP_CONFIGURABLE)

struct NVGcontext;
void nvgqjs_init_with_context(struct NVGcontext* vg);

VISIBLE JSModuleDef* js_init_module_nanovg(JSContext* ctx, const char* module_name);

#endif /* NANOVG_QJS_H */
