#ifndef NANOVG_QJS_H
#define NANOVG_QJS_H
#include <quickjs.h>

#ifdef JS_SHARED_LIBRARY
#define js_init_module_nanovg(x...) js_init_module(x)
#endif

struct NVGcontext;
void js_nanovg_init_with_context(struct NVGcontext* vg);

 __attribute__((visibility("default"))) JSModuleDef* js_init_module_nanovg(JSContext* ctx, const char* module_name);

#endif /* NANOVG_QJS_H */
