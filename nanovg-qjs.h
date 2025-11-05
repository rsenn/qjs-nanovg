#ifndef NANOVG_QJS_H
#define NANOVG_QJS_H

#include <quickjs.h>

#if defined(_WIN32) || defined(__MINGW32__)
#define VISIBLE __declspec(dllexport)
#define HIDDEN
#else
#define VISIBLE __attribute__((visibility("default")))
#define HIDDEN __attribute__((visibility("hidden")))
#endif

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_nanovg
#endif

struct NVGcontext;
void js_nanovg_init_with_context(struct NVGcontext* vg);

VISIBLE JSModuleDef* js_init_module(JSContext* ctx, const char* module_name);

#endif /* NANOVG_QJS_H */
