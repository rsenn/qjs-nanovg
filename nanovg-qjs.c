
#ifdef NANOVG_GLEW
#include <GL/glew.h>
#endif
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#include "nanovg.h"
//#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"

#include "nanovg-qjs.h"

#include <assert.h>
#include <cutils.h>
#include <quickjs.h>
#include <quickjs-libc.h>
#include <stdlib.h>
#include <string.h>

static int
JS_ToFloat32(JSContext* ctx, float* pres, JSValueConst val) {
  double f;
  int ret = JS_ToFloat64(ctx, &f, val);
  if(ret == 0)
    *pres = (float)f;
  return ret;
}

static NVGcontext* g_NVGcontext = NULL;

static JSClassID js_nanovg_paint_class_id;

static void
js_nanovg_paint_finalizer(JSRuntime* rt, JSValue val) {
  NVGpaint* p = JS_GetOpaque(val, js_nanovg_paint_class_id);
  if(p) {
    js_free_rt(rt, p);
  }
}

static JSValue
js_nanovg_paint_ctor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  NVGpaint* p;
  JSValue obj = JS_UNDEFINED;
  JSValue proto;

  p = js_mallocz(ctx, sizeof(*p));
  if(!p)
    return JS_EXCEPTION;

  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if(JS_IsException(proto))
    goto fail;
  obj = JS_NewObjectProtoClass(ctx, proto, js_nanovg_paint_class_id);
  JS_FreeValue(ctx, proto);
  if(JS_IsException(obj))
    goto fail;
  JS_SetOpaque(obj, p);
  return obj;
fail:
  js_free(ctx, p);
  JS_FreeValue(ctx, obj);
  return JS_EXCEPTION;
}

static JSClassDef js_nanovg_paint_class = {
    "Paint",
    .finalizer = js_nanovg_paint_finalizer,
};

static JSValue
js_nanovg_wrap(JSContext* ctx, void* s, JSClassID classID) {
  JSValue obj = JS_NewObjectClass(ctx, classID);
  if(JS_IsException(obj))
    return obj;
  JS_SetOpaque(obj, s);
  return obj;
}

int
GetFloat32PropertyStr(JSContext* ctx, JSValueConst this_obj, const char* prop, float* pres) {
  JSValue p = JS_GetPropertyStr(ctx, this_obj, prop);
  int ret = JS_ToFloat32(ctx, pres, p);
  JS_FreeValue(ctx, p);
  return ret;
}

int
GetFloat32PropertyUint(JSContext* ctx, JSValueConst this_obj, uint32_t idx, float* pres) {
  JSValue p = JS_GetPropertyUint32(ctx, this_obj, idx);
  int ret = JS_ToFloat32(ctx, pres, p);
  JS_FreeValue(ctx, p);
  return ret;
}

int
js_get_NVGcolor(JSContext* ctx, JSValueConst this_obj, NVGcolor* color) {
  int ret = 0;
  ret = ret || GetFloat32PropertyStr(ctx, this_obj, "r", &color->r);
  ret = ret || GetFloat32PropertyStr(ctx, this_obj, "g", &color->g);
  ret = ret || GetFloat32PropertyStr(ctx, this_obj, "b", &color->b);
  ret = ret || GetFloat32PropertyStr(ctx, this_obj, "a", &color->a);
  return ret;
}

int
js_get_transform(JSContext* ctx, JSValueConst this_obj, float* xform) {
  int ret = 0;
  ret = ret || GetFloat32PropertyUint(ctx, this_obj, 0, &xform[0]);
  ret = ret || GetFloat32PropertyUint(ctx, this_obj, 1, &xform[1]);
  ret = ret || GetFloat32PropertyUint(ctx, this_obj, 2, &xform[2]);
  ret = ret || GetFloat32PropertyUint(ctx, this_obj, 3, &xform[3]);
  ret = ret || GetFloat32PropertyUint(ctx, this_obj, 4, &xform[4]);
  ret = ret || GetFloat32PropertyUint(ctx, this_obj, 5, &xform[5]);
  return ret;
}

JSValue
js_NVGcolor_obj(JSContext* ctx, NVGcolor color) {
  JSValue obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, obj, "r", JS_NewFloat64(ctx, color.r));
  JS_SetPropertyStr(ctx, obj, "g", JS_NewFloat64(ctx, color.g));
  JS_SetPropertyStr(ctx, obj, "b", JS_NewFloat64(ctx, color.b));
  JS_SetPropertyStr(ctx, obj, "a", JS_NewFloat64(ctx, color.a));
  return obj;
}
#define FUNC(fn) static JSValue js_nanovg_##fn(JSContext* ctx, JSValueConst this_value, int argc, JSValueConst* argv)

#ifdef NANOVG_GL2
FUNC(CreateGL2) {
  int32_t flags = 0;
  JS_ToInt32(ctx, &flags, argv[0]);

  return JS_NewBool(ctx, !!(g_NVGcontext = nvgCreateGL2(flags)));
}
FUNC(DeleteGL2) {
  if(g_NVGcontext) {
    nvgDeleteGL2(g_NVGcontext);
    g_NVGcontext = 0;
  }
  return JS_UNDEFINED;
}
#endif

#ifdef NANOVG_GL3
FUNC(CreateGL3) {
  int32_t flags = 0;
  JS_ToInt32(ctx, &flags, argv[0]);

  glewExperimental = GL_TRUE;
  if(glewInit() != GLEW_OK) {
    printf("Could not init glew.\n");
    return JS_EXCEPTION;
  }

  // GLEW generates GL error because it calls glGetString(GL_EXTENSIONS), we'll consume it here.
  glGetError();

  return JS_NewBool(ctx, !!(g_NVGcontext = nvgCreateGL3(flags)));
}
FUNC(DeleteGL3) {
  if(g_NVGcontext) {
    nvgDeleteGL3(g_NVGcontext);
    g_NVGcontext = 0;
  }
  return JS_UNDEFINED;
}
#endif

FUNC(BeginFrame) {
  double w, h, ratio;

  JS_ToFloat64(ctx, &w, argv[0]);
  JS_ToFloat64(ctx, &h, argv[1]);
  JS_ToFloat64(ctx, &ratio, argv[2]);

  nvgBeginFrame(g_NVGcontext, w, h, ratio);
  return JS_UNDEFINED;
}

FUNC(EndFrame) {
  nvgEndFrame(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(CancelFrame) {
  nvgCancelFrame(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(Save) {
  nvgSave(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(Restore) {
  nvgReset(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(ShapeAntiAlias) {
  nvgShapeAntiAlias(g_NVGcontext, JS_ToBool(ctx, argv[0]));
  return JS_UNDEFINED;
}

FUNC(Rect) {
  if(argc != 4)
    return JS_EXCEPTION;
  float x, y, w, h;
  if(JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1]) || JS_ToFloat32(ctx, &w, argv[2]) ||
     JS_ToFloat32(ctx, &h, argv[3]))
    return JS_EXCEPTION;
  nvgRect(g_NVGcontext, x, y, w, h);
  return JS_UNDEFINED;
}

FUNC(Circle) {
  if(argc != 3)
    return JS_EXCEPTION;
  float cx, cy, r;
  if(JS_ToFloat32(ctx, &cx, argv[0]) || JS_ToFloat32(ctx, &cy, argv[1]) || JS_ToFloat32(ctx, &r, argv[2]))
    return JS_EXCEPTION;
  nvgCircle(g_NVGcontext, cx, cy, r);
  return JS_UNDEFINED;
}

FUNC(Ellipse) {
  if(argc != 4)
    return JS_EXCEPTION;
  float cx, cy, rx, ry;
  if(JS_ToFloat32(ctx, &cx, argv[0]) || JS_ToFloat32(ctx, &cy, argv[1]) || JS_ToFloat32(ctx, &rx, argv[2]) ||
     JS_ToFloat32(ctx, &ry, argv[3]))
    return JS_EXCEPTION;
  nvgEllipse(g_NVGcontext, cx, cy, rx, ry);
  return JS_UNDEFINED;
}

FUNC(PathWinding) {
  if(argc != 1)
    return JS_EXCEPTION;
  int dir;
  if(JS_ToInt32(ctx, &dir, argv[0]))
    return JS_EXCEPTION;
  nvgPathWinding(g_NVGcontext, dir);
  return JS_UNDEFINED;
}

FUNC(MoveTo) {
  if(argc != 2)
    return JS_EXCEPTION;
  float x, y;
  if(JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1]))
    return JS_EXCEPTION;
  nvgMoveTo(g_NVGcontext, x, y);
  return JS_UNDEFINED;
}

FUNC(LineTo) {
  if(argc != 2)
    return JS_EXCEPTION;
  float x, y;
  if(JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1]))
    return JS_EXCEPTION;
  nvgLineTo(g_NVGcontext, x, y);
  return JS_UNDEFINED;
}

FUNC(FontBlur) {
  if(argc != 1)
    return JS_EXCEPTION;
  float blur;
  if(JS_ToFloat32(ctx, &blur, argv[0]))
    return JS_EXCEPTION;
  nvgFontBlur(g_NVGcontext, blur);
  return JS_UNDEFINED;
}

FUNC(BeginPath) {
  nvgBeginPath(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(RoundedRect) {
  float x, y, w, h, r;
  if(argc != 5)
    return JS_EXCEPTION;
  if(JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1]) || JS_ToFloat32(ctx, &w, argv[2]) ||
     JS_ToFloat32(ctx, &h, argv[3]) || JS_ToFloat32(ctx, &r, argv[4]))
    return JS_EXCEPTION;
  nvgRoundedRect(g_NVGcontext, x, y, w, h, r);
  return JS_UNDEFINED;
}

FUNC(Scissor) {
  float x, y, w, h;
  JS_ToFloat32(ctx, &x, argv[0]);
  JS_ToFloat32(ctx, &y, argv[1]);
  JS_ToFloat32(ctx, &w, argv[2]);
  JS_ToFloat32(ctx, &h, argv[3]);
  nvgScissor(g_NVGcontext, x, y, w, h);
  return JS_UNDEFINED;
}

FUNC(IntersectScissor) {
  float x, y, w, h;
  JS_ToFloat32(ctx, &x, argv[0]);
  JS_ToFloat32(ctx, &y, argv[1]);
  JS_ToFloat32(ctx, &w, argv[2]);
  JS_ToFloat32(ctx, &h, argv[3]);
  nvgIntersectScissor(g_NVGcontext, x, y, w, h);
  return JS_UNDEFINED;
}

FUNC(ResetScissor) {
  nvgResetScissor(g_NVGcontext);
}
FUNC(FillPaint) {
  if(argc != 1)
    return JS_EXCEPTION;
  NVGpaint* paint = JS_GetOpaque(argv[0], js_nanovg_paint_class_id);
  if(!paint)
    return JS_EXCEPTION;
  nvgFillPaint(g_NVGcontext, *paint);
  return JS_UNDEFINED;
}

FUNC(Fill) {
  nvgFill(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(MiterLimit) {
  float miterLimit;
  JS_ToFloat32(ctx, &miterLimit, argv[0]);
  nvgMiterLimit(g_NVGcontext, miterLimit);
  return JS_UNDEFINED;
}

FUNC(LineCap) {
  int32_t lineCap;
  JS_ToInt32(ctx, &lineCap, argv[0]);
  nvgLineCap(g_NVGcontext, lineCap);
  return JS_UNDEFINED;
}

FUNC(LineJoin) {
  int32_t lineJoin;
  JS_ToInt32(ctx, &lineJoin, argv[0]);
  nvgLineCap(g_NVGcontext, lineJoin);
  return JS_UNDEFINED;
}

FUNC(GlobalAlpha) {
  float alpha;
  JS_ToFloat32(ctx, &alpha, argv[0]);
  nvgGlobalAlpha(g_NVGcontext, alpha);
  return JS_UNDEFINED;
}

FUNC(StrokeColor) {
  if(argc != 1)
    return JS_EXCEPTION;
  NVGcolor color;
  if(js_get_NVGcolor(ctx, argv[0], &color))
    return JS_EXCEPTION;
  nvgStrokeColor(g_NVGcontext, color);
  return JS_UNDEFINED;
}

FUNC(Stroke) {
  nvgStroke(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(StrokePaint) {
  if(argc != 1)
    return JS_EXCEPTION;
  NVGpaint* paint = JS_GetOpaque(argv[0], js_nanovg_paint_class_id);
  if(!paint)
    return JS_EXCEPTION;
  nvgStrokePaint(g_NVGcontext, *paint);
  return JS_UNDEFINED;
}

FUNC(FillColor) {
  if(argc != 1)
    return JS_EXCEPTION;
  NVGcolor color;
  if(js_get_NVGcolor(ctx, argv[0], &color))
    return JS_EXCEPTION;
  nvgFillColor(g_NVGcontext, color);
  return JS_UNDEFINED;
}

FUNC(LinearGradient) {
  float sx, sy, ex, ey;
  NVGcolor icol, ocol;
  if(argc != 6)
    return JS_EXCEPTION;
  if(JS_ToFloat32(ctx, &sx, argv[0]) || JS_ToFloat32(ctx, &sy, argv[1]) || JS_ToFloat32(ctx, &ex, argv[2]) ||
     JS_ToFloat32(ctx, &ey, argv[3]) || js_get_NVGcolor(ctx, argv[4], &icol) || js_get_NVGcolor(ctx, argv[5], &ocol))
    return JS_EXCEPTION;
  NVGpaint paint = nvgLinearGradient(g_NVGcontext, sx, sy, ex, ey, icol, ocol);
  NVGpaint* p = js_mallocz(ctx, sizeof(NVGpaint));
  *p = paint;
  return js_nanovg_wrap(ctx, p, js_nanovg_paint_class_id);
}

FUNC(BoxGradient) {
  float x, y, w, h, r, f;
  NVGcolor icol, ocol;
  if(argc != 8)
    return JS_EXCEPTION;
  if(JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1]) || JS_ToFloat32(ctx, &w, argv[2]) ||
     JS_ToFloat32(ctx, &h, argv[3]) || JS_ToFloat32(ctx, &r, argv[4]) || JS_ToFloat32(ctx, &f, argv[5]) ||
     js_get_NVGcolor(ctx, argv[6], &icol) || js_get_NVGcolor(ctx, argv[7], &ocol))
    return JS_EXCEPTION;
  NVGpaint paint = nvgBoxGradient(g_NVGcontext, x, y, w, h, r, f, icol, ocol);
  NVGpaint* p = js_mallocz(ctx, sizeof(NVGpaint));
  *p = paint;
  return js_nanovg_wrap(ctx, p, js_nanovg_paint_class_id);
}

FUNC(RadialGradient) {
  float cx, cy, inr, outr;
  NVGcolor icol, ocol;
  if(argc != 6)
    return JS_EXCEPTION;
  if(JS_ToFloat32(ctx, &cx, argv[0]) || JS_ToFloat32(ctx, &cy, argv[1]) || JS_ToFloat32(ctx, &inr, argv[2]) ||
     JS_ToFloat32(ctx, &outr, argv[3]) || js_get_NVGcolor(ctx, argv[4], &icol) || js_get_NVGcolor(ctx, argv[5], &ocol))
    return JS_EXCEPTION;
  NVGpaint paint = nvgRadialGradient(g_NVGcontext, cx, cy, inr, outr, icol, ocol);
  NVGpaint* p = js_mallocz(ctx, sizeof(NVGpaint));
  *p = paint;
  return js_nanovg_wrap(ctx, p, js_nanovg_paint_class_id);
}

FUNC(TextBounds) {
  float x, y;
  const char* str = NULL;
  if(argc != 3)
    return JS_EXCEPTION;
  if(JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1]))
    return JS_EXCEPTION;
  str = JS_ToCString(ctx, argv[2]);
  if(!str)
    return JS_EXCEPTION;
  float ret = nvgTextBounds(g_NVGcontext, x, y, str, NULL, NULL);
  return JS_NewFloat64(ctx, ret);
}

FUNC(TextBounds2) {
  float x, y;
  const char* str = NULL;
  if(argc != 3)
    return JS_EXCEPTION;
  if(JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1]))
    return JS_EXCEPTION;
  str = JS_ToCString(ctx, argv[2]);
  if(!str)
    return JS_EXCEPTION;
  float bounds[4] = {};
  float tw = nvgTextBounds(g_NVGcontext, x, y, str, NULL, bounds);
  JSValue e = JS_NewObject(ctx);
  JS_DefinePropertyValueStr(ctx, e, "width", JS_NewFloat64(ctx, tw), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, e, "height", JS_NewFloat64(ctx, bounds[3] - bounds[1]), JS_PROP_C_W_E);
  return e;
}

FUNC(FontSize) {
  if(argc != 1)
    return JS_EXCEPTION;
  double size;
  if(JS_ToFloat64(ctx, &size, argv[0]))
    return JS_EXCEPTION;
  nvgFontSize(g_NVGcontext, (float)size);
  return JS_UNDEFINED;
}

FUNC(FontFace) {
  if(argc != 1)
    return JS_EXCEPTION;
  const char* str = JS_ToCString(ctx, argv[0]);
  if(!str)
    return JS_EXCEPTION;
  nvgFontFace(g_NVGcontext, str);
  JS_FreeCString(ctx, str);
  return JS_UNDEFINED;
}

FUNC(TextAlign) {
  if(argc != 1)
    return JS_EXCEPTION;
  int align;
  if(JS_ToInt32(ctx, &align, argv[0]))
    return JS_EXCEPTION;
  nvgTextAlign(g_NVGcontext, align);
  return JS_UNDEFINED;
}

FUNC(Text) {
  int x, y;
  const char* str = NULL;
  if(argc != 3)
    return JS_EXCEPTION;
  if(JS_ToInt32(ctx, &x, argv[0]))
    goto fail;
  if(JS_ToInt32(ctx, &y, argv[1]))
    goto fail;
  str = JS_ToCString(ctx, argv[2]);
  float ret = nvgText(g_NVGcontext, x, y, str, NULL);
  return JS_NewFloat64(ctx, ret);
fail:
  JS_FreeCString(ctx, str);
  return JS_EXCEPTION;
}

FUNC(RGB) {
  int32_t r, g, b;
  NVGcolor color;
  JS_ToInt32(ctx, &r, argv[0]);
  JS_ToInt32(ctx, &g, argv[1]);
  JS_ToInt32(ctx, &b, argv[2]);
  color = nvgRGB(r, g, b);
  return js_NVGcolor_obj(ctx, color);
}

FUNC(RGBf) {
  double r, g, b;
  NVGcolor color;
  JS_ToFloat64(ctx, &r, argv[0]);
  JS_ToFloat64(ctx, &g, argv[1]);
  JS_ToFloat64(ctx, &b, argv[2]);
  color = nvgRGBf(r, g, b);
  return js_NVGcolor_obj(ctx, color);
}

FUNC(RGBA) {
  int32_t r, g, b, a;
  NVGcolor color;
  JS_ToInt32(ctx, &r, argv[0]);
  JS_ToInt32(ctx, &g, argv[1]);
  JS_ToInt32(ctx, &b, argv[2]);
  JS_ToInt32(ctx, &a, argv[3]);
  color = nvgRGBA(r, g, b, a);
  return js_NVGcolor_obj(ctx, color);
}

FUNC(RGBAf) {
  double r, g, b, a;
  NVGcolor color;
  JS_ToFloat64(ctx, &r, argv[0]);
  JS_ToFloat64(ctx, &g, argv[1]);
  JS_ToFloat64(ctx, &b, argv[2]);
  JS_ToFloat64(ctx, &a, argv[3]);
  color = nvgRGBAf(r, g, b, a);
  return js_NVGcolor_obj(ctx, color);
}

FUNC(HSL) {
  int32_t h, s, l;
  NVGcolor color;
  JS_ToInt32(ctx, &h, argv[0]);
  JS_ToInt32(ctx, &s, argv[1]);
  JS_ToInt32(ctx, &l, argv[2]);
  color = nvgHSL(h, s, l);
  return js_NVGcolor_obj(ctx, color);
}

FUNC(HSLA) {
  int32_t h, s, l, a;
  NVGcolor color;
  JS_ToInt32(ctx, &h, argv[0]);
  JS_ToInt32(ctx, &s, argv[1]);
  JS_ToInt32(ctx, &l, argv[2]);
  JS_ToInt32(ctx, &a, argv[2]);
  color = nvgHSLA(h, s, l, a);
  return js_NVGcolor_obj(ctx, color);
}

FUNC(StrokeWidth) {
  double width;
  JS_ToFloat64(ctx, &width, argv[0]);
  nvgStrokeWidth(g_NVGcontext, width);
  return JS_UNDEFINED;
}

FUNC(CreateImage) {
  const char* file = JS_ToCString(ctx, argv[0]);
  int32_t flags = 0;

  JS_ToInt32(ctx, &flags, argv[1]);

  return JS_NewInt32(ctx, nvgCreateImage(g_NVGcontext, file, flags));
}

FUNC(CreateImageRGBA) {
  const char* file = JS_ToCString(ctx, argv[0]);
  int32_t width, height, flags;
  uint8_t* ptr;
  size_t len;
  JS_ToInt32(ctx, &width, argv[0]);
  JS_ToInt32(ctx, &height, argv[1]);
  JS_ToInt32(ctx, &flags, argv[2]);
  ptr = JS_GetArrayBuffer(ctx, &len, argv[3]);
  return JS_NewInt32(ctx, nvgCreateImageRGBA(g_NVGcontext, width, height, flags, (const unsigned char*)ptr));
}

FUNC(ImageSize) {
  int32_t id = 0;
  int width, height;
  JSValue ret;

  JS_ToInt32(ctx, &id, argv[0]);

  nvgImageSize(g_NVGcontext, id, &width, &height);

  ret = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, ret, 0, JS_NewInt32(ctx, width));
  JS_SetPropertyUint32(ctx, ret, 1, JS_NewInt32(ctx, height));
  return ret;
}

FUNC(DeleteImage) {
  int32_t id = 0;
  JS_ToInt32(ctx, &id, argv[0]);
  nvgDeleteImage(g_NVGcontext, id);
  return JS_UNDEFINED;
}

FUNC(ResetTransform) {
  nvgResetTransform(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(Transform) {
  double a, b, c, d, e, f;
  JS_ToFloat64(ctx, &a, argv[0]);
  JS_ToFloat64(ctx, &b, argv[1]);
  JS_ToFloat64(ctx, &c, argv[2]);
  JS_ToFloat64(ctx, &d, argv[3]);
  JS_ToFloat64(ctx, &e, argv[4]);
  JS_ToFloat64(ctx, &f, argv[5]);
  nvgTransform(g_NVGcontext, a, b, c, d, e, f);
  return JS_UNDEFINED;
}

FUNC(Translate) {
  double x, y;
  JS_ToFloat64(ctx, &x, argv[0]);
  JS_ToFloat64(ctx, &y, argv[1]);
  nvgTranslate(g_NVGcontext, x, y);
  return JS_UNDEFINED;
}

FUNC(Rotate) {
  double angle;
  JS_ToFloat64(ctx, &angle, argv[0]);
  nvgRotate(g_NVGcontext, angle);
  return JS_UNDEFINED;
}
FUNC(SkewX) {
  double angle;
  JS_ToFloat64(ctx, &angle, argv[0]);
  nvgSkewX(g_NVGcontext, angle);
  return JS_UNDEFINED;
}
FUNC(SkewY) {
  double angle;
  JS_ToFloat64(ctx, &angle, argv[0]);
  nvgSkewY(g_NVGcontext, angle);
  return JS_UNDEFINED;
}
FUNC(Scale) {
  double x, y;
  JS_ToFloat64(ctx, &x, argv[0]);
  JS_ToFloat64(ctx, &y, argv[1]);
  nvgScale(g_NVGcontext, x, y);
  return JS_UNDEFINED;
}

FUNC(CurrentTransform) {
  float t[6];
  uint32_t i;
  JSValue obj = argc > 0 ? JS_DupValue(ctx, argv[0]) : JS_NewArray(ctx);

  nvgCurrentTransform(g_NVGcontext, t);

  for(i = 0; i < 6; i++) JS_SetPropertyUint32(ctx, obj, i, JS_NewFloat64(ctx, t[i]));

  return obj;
}

FUNC(TransformIdentity) {
  float t[6];
  uint32_t i;
  JSValue obj = argc > 0 ? JS_DupValue(ctx, argv[0]) : JS_NewArray(ctx);
  nvgTransformIdentity(t);
  for(i = 0; i < 6; i++) JS_SetPropertyUint32(ctx, obj, i, JS_NewFloat64(ctx, t[i]));
  return obj;
}

FUNC(TransformTranslate) {
  float t[6];
  uint32_t i;
  int32_t x, y;
  JSValue obj = argc > 0 ? JS_DupValue(ctx, argv[0]) : JS_NewArray(ctx);
  JS_ToInt32(ctx, &x, argv[1]);
  JS_ToInt32(ctx, &y, argv[2]);
  nvgTransformTranslate(t, x, y);
  for(i = 0; i < 6; i++) JS_SetPropertyUint32(ctx, obj, i, JS_NewFloat64(ctx, t[i]));
  return obj;
}

FUNC(TransformScale) {
  float t[6];
  uint32_t i;
  double x, y;
  JSValue obj = argc > 0 ? JS_DupValue(ctx, argv[0]) : JS_NewArray(ctx);
  JS_ToFloat64(ctx, &x, argv[1]);
  JS_ToFloat64(ctx, &y, argv[2]);
  nvgTransformScale(t, x, y);
  for(i = 0; i < 6; i++) JS_SetPropertyUint32(ctx, obj, i, JS_NewFloat64(ctx, t[i]));
  return obj;
}

FUNC(TransformRotate) {
  float t[6];
  uint32_t i;
  double angle;
  JSValue obj = argc > 0 ? JS_DupValue(ctx, argv[0]) : JS_NewArray(ctx);
  JS_ToFloat64(ctx, &angle, argv[1]);
  nvgTransformRotate(t, angle);
  for(i = 0; i < 6; i++) JS_SetPropertyUint32(ctx, obj, i, JS_NewFloat64(ctx, t[i]));
  return obj;
}

FUNC(TransformMultiply) {
  float dst[6], src[6];
  uint32_t i;
  JSValue obj = JS_DupValue(ctx, argv[0]);

  js_get_transform(ctx, argv[0], dst);
  js_get_transform(ctx, argv[1], src);

  nvgTransformMultiply(dst, src);
  for(i = 0; i < 6; i++) JS_SetPropertyUint32(ctx, obj, i, JS_NewFloat64(ctx, dst[i]));
  return obj;
}

FUNC(TransformPoint) {
  float m[6];
  double srcx, srcy;
  float dstx, dsty;
  uint32_t i;
  JSValue obj = JS_NewArray(ctx);

  js_get_transform(ctx, argv[0], m);
  JS_ToFloat64(ctx, &srcx, argv[1]);
  JS_ToFloat64(ctx, &srcy, argv[2]);

  nvgTransformPoint(&dstx, &dsty, m, srcx, srcy);
  JS_SetPropertyUint32(ctx, obj, 0, JS_NewFloat64(ctx, dstx));
  JS_SetPropertyUint32(ctx, obj, 1, JS_NewFloat64(ctx, dsty));
  return obj;
}

FUNC(ImagePattern) {
  double ox, oy, ex, ey;
  double angle, alpha;
  int32_t image;
  NVGpaint paint, *p;
  JS_ToFloat64(ctx, &ox, argv[0]);
  JS_ToFloat64(ctx, &oy, argv[1]);
  JS_ToFloat64(ctx, &ex, argv[2]);
  JS_ToFloat64(ctx, &ey, argv[3]);
  JS_ToFloat64(ctx, &angle, argv[4]);
  JS_ToInt32(ctx, &image, argv[5]);
  JS_ToFloat64(ctx, &alpha, argv[6]);

  paint = nvgImagePattern(g_NVGcontext, ox, oy, ex, ey, angle, image, alpha);
  p = js_malloc(ctx, sizeof(NVGpaint));

  *p = paint;
  return js_nanovg_wrap(ctx, p, js_nanovg_paint_class_id);
}

/*FUNC(SetNextFillHoverable)
{
    nvgSetNextFillHoverable(g_NVGcontext);
    return JS_UNDEFINED;
}

FUNC(IsFillHovered)
{
    int ret = nvgIsFillHovered(g_NVGcontext);
    return JS_NewBool(ctx, ret);
}

FUNC(IsNextFillClicked)
{
    int ret = nvgIsNextFillClicked(g_NVGcontext);
    return JS_NewBool(ctx, ret);
}
*/

#define _JS_CFUNC_DEF(fn, length) JS_CFUNC_DEF(#fn, length, js_nanovg_##fn)
#define _JS_NANOVG_FLAG(name) JS_PROP_INT32_DEF(#name, NVG_##name, JS_PROP_CONFIGURABLE)

static const JSCFunctionListEntry js_nanovg_funcs[] = {
#ifdef NANOVG_GL2
    _JS_CFUNC_DEF(CreateGL2, 1),
#endif
#ifdef NANOVG_GL3
    _JS_CFUNC_DEF(CreateGL3, 1),
#endif
    _JS_CFUNC_DEF(BeginFrame, 3),
    _JS_CFUNC_DEF(CancelFrame, 0),
    _JS_CFUNC_DEF(EndFrame, 0),
    _JS_CFUNC_DEF(Save, 0),
    _JS_CFUNC_DEF(Restore, 0),
    _JS_CFUNC_DEF(ShapeAntiAlias, 1),
    //_JS_CFUNC_DEF(ClosePath, 0),
    _JS_CFUNC_DEF(Scissor, 4),
    _JS_CFUNC_DEF(IntersectScissor, 4),
    _JS_CFUNC_DEF(ResetScissor, 0),

    _JS_CFUNC_DEF(MiterLimit, 1),
    _JS_CFUNC_DEF(LineCap, 1),
    _JS_CFUNC_DEF(LineJoin, 1),
    _JS_CFUNC_DEF(GlobalAlpha, 1),
    _JS_CFUNC_DEF(StrokeColor, 1),
    _JS_CFUNC_DEF(StrokeWidth, 1),
    _JS_CFUNC_DEF(StrokePaint, 1),
    _JS_CFUNC_DEF(FillColor, 1),
    _JS_CFUNC_DEF(FillPaint, 1),
    _JS_CFUNC_DEF(LinearGradient, 6),
    _JS_CFUNC_DEF(BoxGradient, 8),
    _JS_CFUNC_DEF(RadialGradient, 6),
    _JS_CFUNC_DEF(TextBounds, 3),
    _JS_CFUNC_DEF(TextBounds2, 3),
    _JS_CFUNC_DEF(FontBlur, 1),
    _JS_CFUNC_DEF(FontSize, 1),
    _JS_CFUNC_DEF(FontFace, 1),
    _JS_CFUNC_DEF(TextAlign, 1),
    _JS_CFUNC_DEF(Text, 3),
    _JS_CFUNC_DEF(RGB, 3),
    _JS_CFUNC_DEF(RGBf, 3),
    _JS_CFUNC_DEF(RGBA, 4),
    _JS_CFUNC_DEF(RGBAf, 4),
    _JS_CFUNC_DEF(HSL, 3),
    _JS_CFUNC_DEF(HSLA, 4),
    _JS_CFUNC_DEF(CreateImage, 2),
    _JS_CFUNC_DEF(CreateImageRGBA, 4),
    _JS_CFUNC_DEF(ImageSize, 1),
    _JS_CFUNC_DEF(DeleteImage, 1),
    _JS_CFUNC_DEF(ResetTransform, 0),
    _JS_CFUNC_DEF(Transform, 6),
    _JS_CFUNC_DEF(Translate, 2),
    _JS_CFUNC_DEF(Rotate, 1),
    _JS_CFUNC_DEF(SkewX, 1),
    _JS_CFUNC_DEF(SkewY, 1),
    _JS_CFUNC_DEF(Scale, 2),
    _JS_CFUNC_DEF(CurrentTransform, 1),
    _JS_CFUNC_DEF(TransformIdentity, 1),
    _JS_CFUNC_DEF(TransformTranslate, 3),
    _JS_CFUNC_DEF(TransformScale, 3),
    _JS_CFUNC_DEF(TransformRotate, 2),
    _JS_CFUNC_DEF(TransformMultiply, 2),
    _JS_CFUNC_DEF(TransformPoint, 3),
    _JS_CFUNC_DEF(ImagePattern, 7),
    _JS_CFUNC_DEF(BeginPath, 0),
    _JS_CFUNC_DEF(MoveTo, 2),
    _JS_CFUNC_DEF(LineTo, 2),
    _JS_CFUNC_DEF(Rect, 4),
    _JS_CFUNC_DEF(Circle, 3),
    _JS_CFUNC_DEF(Ellipse, 4),
    _JS_CFUNC_DEF(RoundedRect, 5),
    _JS_CFUNC_DEF(PathWinding, 1),
    _JS_CFUNC_DEF(Stroke, 0),
    _JS_CFUNC_DEF(Fill, 0),
    /*    _JS_CFUNC_DEF(SetNextFillHoverable, 0),
             _JS_CFUNC_DEF(IsFillHovered, 0),
             _JS_CFUNC_DEF(IsNextFillClicked, 0),*/
    _JS_NANOVG_FLAG(PI),
    _JS_NANOVG_FLAG(CCW),
    _JS_NANOVG_FLAG(CW),
    _JS_NANOVG_FLAG(SOLID),
    _JS_NANOVG_FLAG(HOLE),
    _JS_NANOVG_FLAG(BUTT),
    _JS_NANOVG_FLAG(ROUND),
    _JS_NANOVG_FLAG(SQUARE),
    _JS_NANOVG_FLAG(BEVEL),
    _JS_NANOVG_FLAG(MITER),
    _JS_NANOVG_FLAG(ALIGN_LEFT),
    _JS_NANOVG_FLAG(ALIGN_CENTER),
    _JS_NANOVG_FLAG(ALIGN_RIGHT),
    _JS_NANOVG_FLAG(ALIGN_TOP),
    _JS_NANOVG_FLAG(ALIGN_MIDDLE),
    _JS_NANOVG_FLAG(ALIGN_BOTTOM),
    _JS_NANOVG_FLAG(ALIGN_BASELINE),
    _JS_NANOVG_FLAG(ZERO),
    _JS_NANOVG_FLAG(ONE),
    _JS_NANOVG_FLAG(SRC_COLOR),
    _JS_NANOVG_FLAG(ONE_MINUS_SRC_COLOR),
    _JS_NANOVG_FLAG(DST_COLOR),
    _JS_NANOVG_FLAG(ONE_MINUS_DST_COLOR),
    _JS_NANOVG_FLAG(SRC_ALPHA),
    _JS_NANOVG_FLAG(ONE_MINUS_SRC_ALPHA),
    _JS_NANOVG_FLAG(DST_ALPHA),
    _JS_NANOVG_FLAG(ONE_MINUS_DST_ALPHA),
    _JS_NANOVG_FLAG(SRC_ALPHA_SATURATE),
    _JS_NANOVG_FLAG(SOURCE_OVER),
    _JS_NANOVG_FLAG(SOURCE_IN),
    _JS_NANOVG_FLAG(SOURCE_OUT),
    _JS_NANOVG_FLAG(ATOP),
    _JS_NANOVG_FLAG(DESTINATION_OVER),
    _JS_NANOVG_FLAG(DESTINATION_IN),
    _JS_NANOVG_FLAG(DESTINATION_OUT),
    _JS_NANOVG_FLAG(DESTINATION_ATOP),
    _JS_NANOVG_FLAG(LIGHTER),
    _JS_NANOVG_FLAG(COPY),
    _JS_NANOVG_FLAG(XOR),
    _JS_NANOVG_FLAG(IMAGE_GENERATE_MIPMAPS),
    _JS_NANOVG_FLAG(IMAGE_REPEATX),
    _JS_NANOVG_FLAG(IMAGE_REPEATY),
    _JS_NANOVG_FLAG(IMAGE_FLIPY),
    _JS_NANOVG_FLAG(IMAGE_PREMULTIPLIED),
    _JS_NANOVG_FLAG(IMAGE_NEAREST),
    _JS_NANOVG_FLAG(BUTT),
    _JS_NANOVG_FLAG(ROUND),
    _JS_NANOVG_FLAG(SQUARE),
    _JS_NANOVG_FLAG(MITER),
    _JS_NANOVG_FLAG(ROUND),
    _JS_NANOVG_FLAG(BEVEL),
    _JS_NANOVG_FLAG(CCW),
    _JS_NANOVG_FLAG(CW),
    _JS_NANOVG_FLAG(TEXTURE_ALPHA),
    _JS_NANOVG_FLAG(TEXTURE_RGBA)};

static int
js_nanovg_init(JSContext* ctx, JSModuleDef* m) {
  JSValue paint_proto, paint_class;

  JS_NewClassID(&js_nanovg_paint_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_nanovg_paint_class_id, &js_nanovg_paint_class);

  paint_proto = JS_NewObject(ctx);
  JS_SetClassProto(ctx, js_nanovg_paint_class_id, paint_proto);
  paint_class = JS_NewCFunction2(ctx, js_nanovg_paint_ctor, "Paint", 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor(ctx, paint_class, paint_proto);

  JS_SetModuleExport(ctx, m, "Paint", paint_class);
  JS_SetModuleExportList(ctx, m, js_nanovg_funcs, countof(js_nanovg_funcs));
  return 0;
}

JSModuleDef*
js_init_module_nanovg(JSContext* ctx, const char* module_name) {
  JSModuleDef* m;
  m = JS_NewCModule(ctx, module_name, js_nanovg_init);
  if(!m)
    return NULL;
  JS_AddModuleExport(ctx, m, "Paint");
  JS_AddModuleExportList(ctx, m, js_nanovg_funcs, countof(js_nanovg_funcs));
  return m;
}

void
js_nanovg_init_with_context(struct NVGcontext* vg) {
  g_NVGcontext = vg;
}
