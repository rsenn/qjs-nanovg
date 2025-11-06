#ifdef NANOVG_GLEW
#include <GL/glew.h>
#endif

#include "nanovg.h"
#include "nanovg_gl.h"

#include "nanovg-qjs.h"
#include "js-utils.h"

#include <assert.h>

static NVGcontext* g_NVGcontext;
static JSClassID nvgjs_paint_class_id;

static JSValue js_float32array_ctor = JS_UNDEFINED, js_float32array_proto = JS_UNDEFINED;
static JSValue color_ctor = JS_UNDEFINED, color_proto = JS_UNDEFINED;
static JSValue matrix_ctor = JS_UNDEFINED, matrix_proto = JS_UNDEFINED;

static float*
nvgjs_float32v(JSContext* ctx, size_t min_size, JSValueConst value) {
  size_t length;
  float* ptr;

  if((ptr = js_float32v_ptr(ctx, &length, value)))
    if(length >= min_size)
      return ptr;

  return 0;
}

static size_t
nvgjs_u8offset(const char* str, size_t len, size_t charpos) {
  const uint8_t *p = (const uint8_t*)str, *e = (const uint8_t*)str + len;

  for(size_t i = 0; i < charpos && p < e; i++) {
    if(unicode_from_utf8(p, e - p, &p) == -1)
      break;

    i++;
  }

  return (const char*)p - str;
}

static const char* const nvgjs_color_keys[] = {"r", "g", "b", "a"};

static int
nvgjs_tocolor(JSContext* ctx, NVGcolor* color, JSValueConst value) {
  return js_tofloat32v(ctx, color->rgba, 4, nvgjs_color_keys, value);
}

static JSValue
nvgjs_wrap(JSContext* ctx, void* s, JSClassID classID) {
  JSValue obj = JS_NewObjectClass(ctx, classID);

  if(JS_IsException(obj))
    return obj;

  JS_SetOpaque(obj, s);
  return obj;
}

static const char* const nvgjs_matrix_keys[] = {"a", "b", "c", "d", "e", "f"};

static int
nvgjs_tomatrix(JSContext* ctx, float matrix[6], JSValueConst value) {
  return js_tofloat32v(ctx, matrix, 6, nvgjs_matrix_keys, value);
}

static int
nvgjs_matrix_copy(JSContext* ctx, JSValueConst value, const float matrix[6]) {
  return js_fromfloat32v(ctx, matrix, 6, nvgjs_matrix_keys, value);
}

static int
nvgjs_matrix_arguments(JSContext* ctx, float matrix[6], int argc, JSValueConst argv[]) {
  int i = 0;

  if(argc >= 6)
    for(i = 0; i < 6; i++)
      if(js_tofloat32(ctx, &matrix[i], argv[i]))
        break;

  if(i == 6)
    return i;

  return !nvgjs_tomatrix(ctx, matrix, argv[0]);
}

static const char* const nvgjs_vector_keys[] = {"x", "y"};

static int
nvgjs_tovector(JSContext* ctx, float vec[2], JSValueConst value) {
  return js_tofloat32v(ctx, vec, 2, nvgjs_vector_keys, value);
}

static int
nvgjs_vector_copy(JSContext* ctx, JSValueConst value, const float vec[2]) {
  return js_fromfloat32v(ctx, vec, 2, nvgjs_vector_keys, value);
}

static int
nvgjs_vector_arguments(JSContext* ctx, float vec[2], int argc, JSValueConst argv[]) {
  int i = 0;

  if(argc >= 2)
    for(i = 0; i < 2; i++)
      if(js_tofloat32(ctx, &vec[i], argv[i]))
        break;

  if(i == 2)
    return i;

  return !nvgjs_tovector(ctx, vec, argv[0]);
}

static JSValue
nvgjs_color_new(JSContext* ctx, NVGcolor color) {
  JSValue buf = JS_NewArrayBufferCopy(ctx, (const void*)&color, sizeof(color));
  JSValue obj = JS_CallConstructor(ctx, js_float32array_ctor, 1, &buf);
  JS_FreeValue(ctx, buf);
  JS_SetPrototype(ctx, obj, color_proto);

  return obj;
}

static JSValue
nvgjs_color_get(JSContext* ctx, JSValueConst this_val, int magic) {
  return JS_GetPropertyUint32(ctx, this_val, magic);
}

static JSValue
nvgjs_color_set(JSContext* ctx, JSValueConst this_val, JSValueConst value, int magic) {
  JS_SetPropertyUint32(ctx, this_val, magic, JS_DupValue(ctx, value));
  return JS_UNDEFINED;
}

static const JSCFunctionListEntry nvgjs_color_methods[] = {
    JS_CGETSET_MAGIC_DEF("r", nvgjs_color_get, nvgjs_color_set, 0),
    JS_CGETSET_MAGIC_DEF("g", nvgjs_color_get, nvgjs_color_set, 1),
    JS_CGETSET_MAGIC_DEF("b", nvgjs_color_get, nvgjs_color_set, 2),
    JS_CGETSET_MAGIC_DEF("a", nvgjs_color_get, nvgjs_color_set, 3),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "nvgColor", JS_PROP_CONFIGURABLE),
};

static JSValue
nvgjs_matrix_new(JSContext* ctx, float matrix[6]) {
  JSValue buf = JS_NewArrayBufferCopy(ctx, (void*)matrix, 6 * sizeof(float));
  JSValue obj = JS_CallConstructor(ctx, js_float32array_ctor, 1, &buf);
  JS_FreeValue(ctx, buf);
  JS_SetPrototype(ctx, obj, matrix_proto);

  return obj;
}

static JSValue
nvgjs_matrix_get(JSContext* ctx, JSValueConst this_val, int magic) {
  return JS_GetPropertyUint32(ctx, this_val, magic);
}

static JSValue
nvgjs_matrix_set(JSContext* ctx, JSValueConst this_val, JSValueConst value, int magic) {
  JS_SetPropertyUint32(ctx, this_val, magic, JS_DupValue(ctx, value));
  return JS_UNDEFINED;
}

enum {
  MATRIX_TRANSFORM,
};

static JSValue
nvgjs_matrix_method(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
  float* matrix;

  if(!(matrix = nvgjs_float32v(ctx, 6, this_val)))
    return JS_EXCEPTION;

  switch(magic) {
    case MATRIX_TRANSFORM: {
      for(int i = 0; i < argc; i++) {
        float vec[2], *ptr;

        if(!(ptr = nvgjs_float32v(ctx, 2, argv[i])))
          if(nvgjs_tovector(ctx, (ptr = vec), argv[i]))
            return JS_EXCEPTION;

        nvgTransformPoint(&ptr[0], &ptr[1], matrix, ptr[0], ptr[1]);

        if(ptr == vec)
          nvgjs_vector_copy(ctx, argv[i], vec);
      }

      break;
    }
  }

  return JS_UNDEFINED;
}

static const JSCFunctionListEntry nvgjs_matrix_methods[] = {
    JS_CFUNC_MAGIC_DEF("transform", 1, nvgjs_matrix_method, MATRIX_TRANSFORM),
    JS_CGETSET_MAGIC_DEF("a", nvgjs_matrix_get, nvgjs_matrix_set, 0),
    JS_CGETSET_MAGIC_DEF("b", nvgjs_matrix_get, nvgjs_matrix_set, 1),
    JS_CGETSET_MAGIC_DEF("c", nvgjs_matrix_get, nvgjs_matrix_set, 2),
    JS_CGETSET_MAGIC_DEF("d", nvgjs_matrix_get, nvgjs_matrix_set, 3),
    JS_CGETSET_MAGIC_DEF("e", nvgjs_matrix_get, nvgjs_matrix_set, 4),
    JS_CGETSET_MAGIC_DEF("f", nvgjs_matrix_get, nvgjs_matrix_set, 5),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "nvgMatrix", JS_PROP_CONFIGURABLE),
};

static JSValue
nvgjs_paint_new(JSContext* ctx, NVGpaint p) {
  JSValue obj;
  NVGpaint* ptr;

  if(!(ptr = js_malloc(ctx, sizeof(NVGpaint))))
    return JS_EXCEPTION;

  *ptr = p;

  obj = JS_NewObjectClass(ctx, nvgjs_paint_class_id);
  if(JS_IsException(obj))
    return obj;

  JS_SetOpaque(obj, ptr);
  return obj;
}

static void
nvgjs_paint_finalizer(JSRuntime* rt, JSValue val) {
  NVGpaint* p;

  if((p = JS_GetOpaque(val, nvgjs_paint_class_id))) {
    js_free_rt(rt, p);
  }
}

static JSValue
nvgjs_paint_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  NVGpaint* p;
  JSValue proto, obj = JS_UNDEFINED;

  if(!(p = js_mallocz(ctx, sizeof(*p))))
    return JS_EXCEPTION;

  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if(JS_IsException(proto))
    goto fail;

  obj = JS_NewObjectProtoClass(ctx, proto, nvgjs_paint_class_id);
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

static JSClassDef nvgjs_paint_class = {
    "nvgPaint",
    .finalizer = nvgjs_paint_finalizer,
};

static const JSCFunctionListEntry nvgjs_paint_methods[] = {
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "nvgPaint", JS_PROP_CONFIGURABLE),
};

#ifdef NANOVG_GL2
NVGJS_DECL(CreateGL2) {
  int32_t flags = 0;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &flags, argv[0]))
    return JS_EXCEPTION;

  return JS_NewBool(ctx, !!(g_NVGcontext = nvgCreateGL2(flags)));
}

NVGJS_DECL(DeleteGL2) {
  if(g_NVGcontext) {
    nvgDeleteGL2(g_NVGcontext);
    g_NVGcontext = 0;
  }

  return JS_UNDEFINED;
}
#endif

#ifdef NANOVG_GL3
NVGJS_DECL(CreateGL3) {
  int32_t flags = 0;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &flags, argv[0]))
    return JS_EXCEPTION;

#ifdef NANOVG_GLEW
  glewExperimental = GL_TRUE;

  if(glewInit() != GLEW_OK) {
    return JS_ThrowInternalError(ctx, "nvg.CreateGL3: Could not init glew.\n");
  }
#endif

  // GLEW generates GL error because it calls glGetString(GL_EXTENSIONS), we'll consume it here.
  glGetError();

  return JS_NewBool(ctx, !!(g_NVGcontext = nvgCreateGL3(flags)));
}

NVGJS_DECL(DeleteGL3) {
  if(g_NVGcontext) {
    nvgDeleteGL3(g_NVGcontext);
    g_NVGcontext = 0;
  }

  return JS_UNDEFINED;
}
#endif

NVGJS_DECL(CreateFont) {
  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* file = JS_ToCString(ctx, argv[1]);

  return JS_NewInt32(ctx, nvgCreateFont(g_NVGcontext, name, file));
}

NVGJS_DECL(CreateFontAtIndex) {
  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* file = JS_ToCString(ctx, argv[1]);
  int32_t index;

  if(JS_ToInt32(ctx, &index, argv[2]))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateFontAtIndex(g_NVGcontext, name, file, index));
}

NVGJS_DECL(FindFont) {
  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  const char* name = JS_ToCString(ctx, argv[0]);

  return JS_NewInt32(ctx, nvgFindFont(g_NVGcontext, name));
}

NVGJS_DECL(BeginFrame) {
  double w, h, ratio;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &w, argv[0]) || JS_ToFloat64(ctx, &h, argv[1]) || JS_ToFloat64(ctx, &ratio, argv[2]))
    return JS_EXCEPTION;

  nvgBeginFrame(g_NVGcontext, w, h, ratio);
  return JS_UNDEFINED;
}

NVGJS_DECL(EndFrame) {
  nvgEndFrame(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(CancelFrame) {
  nvgCancelFrame(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(Save) {
  nvgSave(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(Restore) {
  nvgRestore(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(Reset) {
  nvgReset(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(ShapeAntiAlias) {
  nvgShapeAntiAlias(g_NVGcontext, JS_ToBool(ctx, argv[0]));
  return JS_UNDEFINED;
}

NVGJS_DECL(Rect) {
  double x, y, w, h;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgRect(g_NVGcontext, x, y, w, h);
  return JS_UNDEFINED;
}

NVGJS_DECL(Circle) {
  double cx, cy, r;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &r, argv[2]))
    return JS_EXCEPTION;

  nvgCircle(g_NVGcontext, cx, cy, r);
  return JS_UNDEFINED;
}

NVGJS_DECL(Ellipse) {
  double cx, cy, rx, ry;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &rx, argv[2]) || JS_ToFloat64(ctx, &ry, argv[3]))
    return JS_EXCEPTION;

  nvgEllipse(g_NVGcontext, cx, cy, rx, ry);
  return JS_UNDEFINED;
}

NVGJS_DECL(PathWinding) {
  int dir;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &dir, argv[0]))
    return JS_EXCEPTION;

  nvgPathWinding(g_NVGcontext, dir);
  return JS_UNDEFINED;
}

NVGJS_DECL(MoveTo) {
  double x, y;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgMoveTo(g_NVGcontext, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(LineTo) {
  double x, y;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgLineTo(g_NVGcontext, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(BezierTo) {
  double c1x, c1y, c2x, c2y, x, y;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &c1x, argv[0]) || JS_ToFloat64(ctx, &c1y, argv[1]) || JS_ToFloat64(ctx, &c2x, argv[2]) || JS_ToFloat64(ctx, &c2y, argv[3]) || JS_ToFloat64(ctx, &x, argv[4]) || JS_ToFloat64(ctx, &y, argv[5]))
    return JS_EXCEPTION;

  nvgBezierTo(g_NVGcontext, c1x, c1y, c2x, c2y, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(QuadTo) {
  double cx, cy, x, y;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &x, argv[2]) || JS_ToFloat64(ctx, &y, argv[3]))
    return JS_EXCEPTION;

  nvgQuadTo(g_NVGcontext, cx, cy, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(ArcTo) {
  double x1, y1, x2, y2, radius;

  if(argc < 5)
    return JS_ThrowInternalError(ctx, "need 5 arguments");

  if(JS_ToFloat64(ctx, &x1, argv[0]) || JS_ToFloat64(ctx, &y1, argv[1]) || JS_ToFloat64(ctx, &x2, argv[2]) || JS_ToFloat64(ctx, &y2, argv[3]) || JS_ToFloat64(ctx, &radius, argv[4]))
    return JS_EXCEPTION;

  nvgArcTo(g_NVGcontext, x1, y1, x2, y2, radius);
  return JS_UNDEFINED;
}

NVGJS_DECL(Arc) {
  double cx, cy, r, a0, a1;
  int32_t dir;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &r, argv[2]) || JS_ToFloat64(ctx, &a0, argv[3]) || JS_ToFloat64(ctx, &a1, argv[4]) || JS_ToInt32(ctx, &dir, argv[5]))
    return JS_EXCEPTION;

  nvgArc(g_NVGcontext, cx, cy, r, a0, a1, dir);
  return JS_UNDEFINED;
}

NVGJS_DECL(ClosePath) {
  nvgClosePath(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(FontSize) {
  double size;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &size, argv[0]))
    return JS_EXCEPTION;

  nvgFontSize(g_NVGcontext, size);
  return JS_UNDEFINED;
}

NVGJS_DECL(FontBlur) {
  double blur;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &blur, argv[0]))
    return JS_EXCEPTION;

  nvgFontBlur(g_NVGcontext, blur);
  return JS_UNDEFINED;
}

NVGJS_DECL(TextLetterSpacing) {
  double spacing;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &spacing, argv[0]))
    return JS_EXCEPTION;

  nvgTextLetterSpacing(g_NVGcontext, spacing);
  return JS_UNDEFINED;
}

NVGJS_DECL(TextLineHeight) {
  double height;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &height, argv[0]))
    return JS_EXCEPTION;

  nvgTextLineHeight(g_NVGcontext, height);
  return JS_UNDEFINED;
}

NVGJS_DECL(BeginPath) {
  nvgBeginPath(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(RoundedRect) {
  double x, y, w, h, r;

  if(argc < 5)
    return JS_ThrowInternalError(ctx, "need 5 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &r, argv[4]))
    return JS_EXCEPTION;

  nvgRoundedRect(g_NVGcontext, x, y, w, h, r);
  return JS_UNDEFINED;
}

NVGJS_DECL(RoundedRectVarying) {
  double x, y, w, h, rtl, rtr, rbr, rbl;

  if(argc < 8)
    return JS_ThrowInternalError(ctx, "need 8 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &rtl, argv[4]) || JS_ToFloat64(ctx, &rtr, argv[5]) || JS_ToFloat64(ctx, &rbr, argv[6]) || JS_ToFloat64(ctx, &rbl, argv[7]))
    return JS_EXCEPTION;

  nvgRoundedRectVarying(g_NVGcontext, x, y, w, h, rtl, rtr, rbr, rbl);
  return JS_UNDEFINED;
}

NVGJS_DECL(Scissor) {
  double x, y, w, h;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgScissor(g_NVGcontext, x, y, w, h);
  return JS_UNDEFINED;
}

NVGJS_DECL(IntersectScissor) {
  double x, y, w, h;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgIntersectScissor(g_NVGcontext, x, y, w, h);

  return JS_UNDEFINED;
}

NVGJS_DECL(ResetScissor) {
  nvgResetScissor(g_NVGcontext);
}

NVGJS_DECL(FillPaint) {
  NVGpaint* paint;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(!(paint = JS_GetOpaque2(ctx, argv[0], nvgjs_paint_class_id)))
    return JS_EXCEPTION;

  nvgFillPaint(g_NVGcontext, *paint);
  return JS_UNDEFINED;
}

NVGJS_DECL(Fill) {
  nvgFill(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(MiterLimit) {
  double miterLimit;

  if(JS_ToFloat64(ctx, &miterLimit, argv[0]))
    return JS_EXCEPTION;

  nvgMiterLimit(g_NVGcontext, miterLimit);
  return JS_UNDEFINED;
}

NVGJS_DECL(LineCap) {
  int32_t lineCap;

  if(JS_ToInt32(ctx, &lineCap, argv[0]))
    return JS_EXCEPTION;

  nvgLineCap(g_NVGcontext, lineCap);
  return JS_UNDEFINED;
}

NVGJS_DECL(LineJoin) {
  int32_t lineJoin;

  if(JS_ToInt32(ctx, &lineJoin, argv[0]))
    return JS_EXCEPTION;

  nvgLineCap(g_NVGcontext, lineJoin);
  return JS_UNDEFINED;
}

NVGJS_DECL(GlobalAlpha) {
  double alpha;

  if(JS_ToFloat64(ctx, &alpha, argv[0]))
    return JS_EXCEPTION;

  nvgGlobalAlpha(g_NVGcontext, alpha);
  return JS_UNDEFINED;
}

NVGJS_DECL(StrokeColor) {
  NVGcolor color;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(nvgjs_tocolor(ctx, &color, argv[0]))
    return JS_EXCEPTION;

  nvgStrokeColor(g_NVGcontext, color);
  return JS_UNDEFINED;
}

NVGJS_DECL(Stroke) {
  nvgStroke(g_NVGcontext);
  return JS_UNDEFINED;
}

NVGJS_DECL(StrokePaint) {
  NVGpaint* paint;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(!(paint = JS_GetOpaque(argv[0], nvgjs_paint_class_id)))
    return JS_EXCEPTION;

  nvgStrokePaint(g_NVGcontext, *paint);
  return JS_UNDEFINED;
}

NVGJS_DECL(FillColor) {
  NVGcolor color;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(nvgjs_tocolor(ctx, &color, argv[0]))
    return JS_EXCEPTION;

  nvgFillColor(g_NVGcontext, color);
  return JS_UNDEFINED;
}

NVGJS_DECL(LinearGradient) {
  double sx, sy, ex, ey;
  NVGcolor icol, ocol;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &sx, argv[0]) || JS_ToFloat64(ctx, &sy, argv[1]) || JS_ToFloat64(ctx, &ex, argv[2]) || JS_ToFloat64(ctx, &ey, argv[3]) || nvgjs_tocolor(ctx, &icol, argv[4]) || nvgjs_tocolor(ctx, &ocol, argv[5]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgLinearGradient(g_NVGcontext, sx, sy, ex, ey, icol, ocol));
}

NVGJS_DECL(BoxGradient) {
  double x, y, w, h, r, f;
  NVGcolor icol, ocol;

  if(argc < 8)
    return JS_ThrowInternalError(ctx, "need 8 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &r, argv[4]) || JS_ToFloat64(ctx, &f, argv[5]) || nvgjs_tocolor(ctx, &icol, argv[6]) || nvgjs_tocolor(ctx, &ocol, argv[7]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgBoxGradient(g_NVGcontext, x, y, w, h, r, f, icol, ocol));
}

NVGJS_DECL(RadialGradient) {
  double cx, cy, inr, outr;
  NVGcolor icol, ocol;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &inr, argv[2]) || JS_ToFloat64(ctx, &outr, argv[3]) || nvgjs_tocolor(ctx, &icol, argv[4]) || nvgjs_tocolor(ctx, &ocol, argv[5]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgRadialGradient(g_NVGcontext, cx, cy, inr, outr, icol, ocol));
}

NVGJS_DECL(TextAlign) {
  int align;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &align, argv[0]))
    return JS_EXCEPTION;

  nvgTextAlign(g_NVGcontext, align);
  return JS_UNDEFINED;
}

NVGJS_DECL(FontFace) {
  const char* str;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(!(str = JS_ToCString(ctx, argv[0])))
    return JS_EXCEPTION;

  nvgFontFace(g_NVGcontext, str);
  JS_FreeCString(ctx, str);
  return JS_UNDEFINED;
}

NVGJS_DECL(Text) {
  int x, y;
  const char *str, *end = 0;
  size_t len;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToInt32(ctx, &x, argv[0]) || JS_ToInt32(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  if(!(str = JS_ToCStringLen(ctx, &len, argv[2])))
    return JS_EXCEPTION;

  if(argc > 3) {
    uint64_t u;
    if(!JS_ToIndex(ctx, &u, argv[3]))
      end = str + nvgjs_u8offset(str, len, u);
  }

  float ret = nvgText(g_NVGcontext, x, y, str, end);

  JS_FreeCString(ctx, str);

  return JS_NewFloat64(ctx, ret);
}

NVGJS_DECL(TextBox) {
  int x, y;
  double breakRowWidth;
  const char *str, *end = 0;
  size_t len;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToInt32(ctx, &x, argv[0]) || JS_ToInt32(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &breakRowWidth, argv[2]))
    return JS_EXCEPTION;

  if(!(str = JS_ToCStringLen(ctx, &len, argv[3])))
    return JS_EXCEPTION;

  if(argc > 4) {
    uint64_t u;
    if(!JS_ToIndex(ctx, &u, argv[4]))
      end = str + nvgjs_u8offset(str, len, u);
  }

  nvgTextBox(g_NVGcontext, x, y, breakRowWidth, str, end);

  JS_FreeCString(ctx, str);

  return JS_UNDEFINED;
}

NVGJS_DECL(TextBounds) {
  double x, y;
  const char *str, *end = 0;
  size_t len;
  float bounds[4];

  if(argc < 5)
    return JS_ThrowInternalError(ctx, "need 5 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  if(!(str = JS_ToCStringLen(ctx, &len, argv[2])))
    return JS_EXCEPTION;

  if(!(JS_IsNull(argv[3]) || JS_IsUndefined(argv[3]))) {
    uint64_t u;
    if(!JS_ToIndex(ctx, &u, argv[3]))
      end = str + nvgjs_u8offset(str, len, u);
  }

  float ret = nvgTextBounds(g_NVGcontext, x, y, str, end, bounds);

  JS_FreeCString(ctx, str);

  js_float32v_store(ctx, bounds, countof(bounds), (const char* const[]){"xmin", "ymin", "xmax", "ymax"}, argv[4]);

  return JS_NewFloat64(ctx, ret);
}

NVGJS_DECL(TextBoxBounds) {
  int x, y;
  double breakRowWidth;
  const char *str, *end = 0;
  size_t len;
  float bounds[4];

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToInt32(ctx, &x, argv[0]) || JS_ToInt32(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &breakRowWidth, argv[2]))
    return JS_EXCEPTION;

  if(!(str = JS_ToCStringLen(ctx, &len, argv[3])))
    return JS_EXCEPTION;

  if(argc > 4) {
    uint64_t u;
    if(!JS_ToIndex(ctx, &u, argv[4]))
      end = str + nvgjs_u8offset(str, len, u);
  }

  nvgTextBoxBounds(g_NVGcontext, x, y, breakRowWidth, str, end, bounds);

  JS_FreeCString(ctx, str);

  js_float32v_store(ctx, bounds, countof(bounds), (const char* const[]){"xmin", "ymin", "xmax", "ymax"}, argv[5]);

  return JS_UNDEFINED;
}

NVGJS_DECL(TextBounds2) {
  double x, y;
  const char* str;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  if(!(str = JS_ToCString(ctx, argv[2])))
    return JS_EXCEPTION;

  {
    float bounds[4] = {};
    float tw = nvgTextBounds(g_NVGcontext, x, y, str, NULL, bounds);

    JSValue e = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, e, "width", JS_NewFloat64(ctx, tw), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, e, "height", JS_NewFloat64(ctx, bounds[3] - bounds[1]), JS_PROP_C_W_E);
    return e;
  }
}

NVGJS_DECL(RGB) {
  int32_t r, g, b;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToInt32(ctx, &r, argv[0]) || JS_ToInt32(ctx, &g, argv[1]) || JS_ToInt32(ctx, &b, argv[2]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGB(r, g, b));
}

NVGJS_DECL(RGBf) {
  double r, g, b;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &r, argv[0]) || JS_ToFloat64(ctx, &g, argv[1]) || JS_ToFloat64(ctx, &b, argv[2]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGBf(r, g, b));
}

NVGJS_DECL(RGBA) {
  int32_t r, g, b, a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToInt32(ctx, &r, argv[0]) || JS_ToInt32(ctx, &g, argv[1]) || JS_ToInt32(ctx, &b, argv[2]) || JS_ToInt32(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGBA(r, g, b, a));
}

NVGJS_DECL(RGBAf) {
  double r, g, b, a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &r, argv[0]) || JS_ToFloat64(ctx, &g, argv[1]) || JS_ToFloat64(ctx, &b, argv[2]) || JS_ToFloat64(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGBAf(r, g, b, a));
}

NVGJS_DECL(LerpRGBA) {
  NVGcolor c0, c1;
  double u;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(nvgjs_tocolor(ctx, &c0, argv[0]) || nvgjs_tocolor(ctx, &c1, argv[1]) || JS_ToFloat64(ctx, &u, argv[2]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgLerpRGBA(c0, c1, u));
}

NVGJS_DECL(TransRGBA) {
  NVGcolor c;
  int32_t a;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(nvgjs_tocolor(ctx, &c, argv[0]) || JS_ToInt32(ctx, &a, argv[1]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgTransRGBA(c, a));
}

NVGJS_DECL(TransRGBAf) {
  NVGcolor c;
  double a;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(nvgjs_tocolor(ctx, &c, argv[0]) || JS_ToFloat64(ctx, &a, argv[1]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgTransRGBAf(c, a));
}

NVGJS_DECL(HSL) {
  double h, s, l;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &h, argv[0]) || JS_ToFloat64(ctx, &s, argv[1]) || JS_ToFloat64(ctx, &l, argv[2]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgHSL(h, s, l));
}

NVGJS_DECL(HSLA) {
  double h, s, l;
  int32_t a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &h, argv[0]) || JS_ToFloat64(ctx, &s, argv[1]) || JS_ToFloat64(ctx, &l, argv[2]) || JS_ToInt32(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgHSLA(h, s, l, a));
}

NVGJS_DECL(StrokeWidth) {
  double width;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &width, argv[0]))
    return JS_EXCEPTION;

  nvgStrokeWidth(g_NVGcontext, width);
  return JS_UNDEFINED;
}

NVGJS_DECL(CreateImage) {
  const char* file;
  int32_t flags = 0;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(!(file = JS_ToCString(ctx, argv[0])))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &flags, argv[1]))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateImage(g_NVGcontext, file, flags));
}

NVGJS_DECL(CreateImageMem) {
  int32_t flags;
  uint8_t* ptr;
  size_t len;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToInt32(ctx, &flags, argv[0]))
    return JS_EXCEPTION;

  if(!(ptr = JS_GetArrayBuffer(ctx, &len, argv[1])))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateImageMem(g_NVGcontext, flags, (void*)ptr, len));
}

NVGJS_DECL(CreateImageRGBA) {
  int32_t width, height, flags;
  uint8_t* ptr;
  size_t len;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToInt32(ctx, &width, argv[0]) || JS_ToInt32(ctx, &height, argv[1]) || JS_ToInt32(ctx, &flags, argv[2]))
    return JS_EXCEPTION;

  if(!(ptr = JS_GetArrayBuffer(ctx, &len, argv[3])))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateImageRGBA(g_NVGcontext, width, height, flags, (void*)ptr));
}

NVGJS_DECL(UpdateImage) {
  int32_t image;
  size_t len;
  uint8_t* ptr;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToInt32(ctx, &image, argv[0]))
    return JS_EXCEPTION;

  if(!(ptr = JS_GetArrayBuffer(ctx, &len, argv[1])))
    return JS_EXCEPTION;

  nvgUpdateImage(g_NVGcontext, image, (void*)ptr);

  return JS_UNDEFINED;
}

NVGJS_DECL(ImageSize) {
  int32_t id = 0;
  int width, height;
  JSValue ret;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &id, argv[0]))
    return JS_EXCEPTION;

  nvgImageSize(g_NVGcontext, id, &width, &height);

  ret = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, ret, 0, JS_NewInt32(ctx, width));
  JS_SetPropertyUint32(ctx, ret, 1, JS_NewInt32(ctx, height));
  return ret;
}

NVGJS_DECL(DeleteImage) {
  int32_t id = 0;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &id, argv[0]))
    return JS_EXCEPTION;

  nvgDeleteImage(g_NVGcontext, id);

  return JS_UNDEFINED;
}

NVGJS_DECL(ResetTransform) {
  nvgResetTransform(g_NVGcontext);

  return JS_UNDEFINED;
}

NVGJS_DECL(Transform) {
  float t[6];
  int n;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1/6 arguments");

  while((n = nvgjs_matrix_arguments(ctx, t, argc, argv))) {
    nvgTransform(g_NVGcontext, t[0], t[1], t[2], t[3], t[4], t[5]);

    argc -= n;
    argv += n;
  }

  return JS_UNDEFINED;
}

NVGJS_DECL(Translate) {
  double x, y;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgTranslate(g_NVGcontext, x, y);

  return JS_UNDEFINED;
}

NVGJS_DECL(Rotate) {
  double angle;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgRotate(g_NVGcontext, angle);

  return JS_UNDEFINED;
}

NVGJS_DECL(SkewX) {
  double angle;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgSkewX(g_NVGcontext, angle);

  return JS_UNDEFINED;
}

NVGJS_DECL(SkewY) {
  double angle;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgSkewY(g_NVGcontext, angle);

  return JS_UNDEFINED;
}

NVGJS_DECL(Scale) {
  double x, y;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgScale(g_NVGcontext, x, y);

  return JS_UNDEFINED;
}

NVGJS_DECL(CurrentTransform) {
  float t[6];

  nvgCurrentTransform(g_NVGcontext, t);

  if(argc == 0)
    return nvgjs_matrix_new(ctx, t);

  nvgjs_matrix_copy(ctx, argv[0], t);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformIdentity) {
  float t[6];

  nvgTransformIdentity(t);

  if(magic || argc == 0)
    return nvgjs_matrix_new(ctx, t);

  nvgjs_matrix_copy(ctx, argv[0], t);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformTranslate) {
  float t[6];
  int32_t x, y;
  int i = 0;

  if(!magic)
    if(argc >= 3 && JS_IsObject(argv[0]))
      i++;

  if(argc < 2 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 2 + i);

  if(JS_ToInt32(ctx, &x, argv[i]) || JS_ToInt32(ctx, &y, argv[i + 1]))
    return JS_EXCEPTION;

  nvgTransformTranslate(t, x, y);

  if(i == 0)
    return nvgjs_matrix_new(ctx, t);

  nvgjs_matrix_copy(ctx, argv[0], t);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformScale) {
  float t[6];
  double x, y;
  int i = 0;

  if(!magic)
    if(argc >= 3 && JS_IsObject(argv[0]))
      i++;

  if(argc < 2 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 2 + i);

  if(JS_ToFloat64(ctx, &x, argv[i]) || JS_ToFloat64(ctx, &y, argv[i + 1]))
    return JS_EXCEPTION;

  nvgTransformScale(t, x, y);

  if(i == 0)
    return nvgjs_matrix_new(ctx, t);

  nvgjs_matrix_copy(ctx, argv[0], t);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformRotate) {
  float t[6];
  double angle;
  int i = 0;

  if(!magic)
    if(argc >= 2 && JS_IsObject(argv[0]))
      i++;

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformRotate(t, angle);

  if(i == 0)
    return nvgjs_matrix_new(ctx, t);

  nvgjs_matrix_copy(ctx, argv[0], t);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformSkewX) {
  float t[6];
  double angle;
  int i = 0;

  if(!magic)
    if(argc >= 2 && JS_IsObject(argv[0]))
      i++;

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformSkewX(t, angle);

  if(i == 0)
    return nvgjs_matrix_new(ctx, t);

  nvgjs_matrix_copy(ctx, argv[0], t);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformSkewY) {
  float t[6];
  double angle;
  int i = 0;

  if(!magic)
    if(argc >= 2 && JS_IsObject(argv[0]))
      i++;

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformSkewY(t, angle);

  if(i == 0)
    return nvgjs_matrix_new(ctx, t);

  nvgjs_matrix_copy(ctx, argv[0], t);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformMultiply) {
  float dst[6], src[6];

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(magic) {
    nvgTransformIdentity(dst);
  } else {
    nvgjs_tomatrix(ctx, dst, argv[0]);
    argv++;
    argc--;
  }

  for(int n, i = 0; i < argc && (n = nvgjs_matrix_arguments(ctx, src, argc - i, argv + i)); i += n) {
    nvgTransformMultiply(dst, src);
  }

  if(magic)
    return nvgjs_matrix_new(ctx, dst);

  nvgjs_matrix_copy(ctx, argv[0], dst);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformPremultiply) {
  float dst[6], src[6];

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(magic) {
    nvgTransformIdentity(dst);
  } else {
    nvgjs_tomatrix(ctx, dst, argv[0]);
    argv++;
    argc--;
  }

  for(int n, i = 0; i < argc && (n = nvgjs_matrix_arguments(ctx, src, argc - i, argv + i)); i += n) {
    nvgTransformPremultiply(dst, src);
  }

  if(magic)
    return nvgjs_matrix_new(ctx, dst);

  nvgjs_matrix_copy(ctx, argv[0], dst);
  return JS_UNDEFINED;
}

NVGJS_DECL(TransformInverse) {
  float dst[6], src[6];
  int i = 0;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(!magic)
    if(argc > 1)
      i++;

  nvgjs_tomatrix(ctx, src, argv[i]);

  int ret = nvgTransformInverse(dst, src);

  if(ret) {
    if(i == 0)
      return nvgjs_matrix_new(ctx, dst);

    nvgjs_matrix_copy(ctx, argv[0], dst);
  }

  return JS_NewInt32(ctx, ret);
}

NVGJS_DECL(TransformPoint) {
  float m[6], dst[2], src[2];

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  nvgjs_tovector(ctx, dst, argv[0]);
  nvgjs_tomatrix(ctx, m, argv[1]);
  nvgjs_vector_arguments(ctx, src, argc - 2, argv + 2);
  nvgTransformPoint(&dst[0], &dst[1], m, src[0], src[1]);
  nvgjs_vector_copy(ctx, argv[0], dst);

  return JS_UNDEFINED;
}

NVGJS_DECL(DegToRad) {
  double arg;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &arg, argv[0]))
    return JS_EXCEPTION;

  return JS_NewFloat64(ctx, nvgDegToRad(arg));
}

NVGJS_DECL(RadToDeg) {
  double arg;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &arg, argv[0]))
    return JS_EXCEPTION;

  return JS_NewFloat64(ctx, nvgRadToDeg(arg));
}

NVGJS_DECL(ImagePattern) {
  double ox, oy, ex, ey;
  double angle, alpha;
  int32_t image;
  NVGpaint paint, *p;

  if(argc < 7)
    return JS_ThrowInternalError(ctx, "need 7 arguments");

  if(JS_ToFloat64(ctx, &ox, argv[0]) || JS_ToFloat64(ctx, &oy, argv[1]) || JS_ToFloat64(ctx, &ex, argv[2]) || JS_ToFloat64(ctx, &ey, argv[3]) || JS_ToFloat64(ctx, &angle, argv[4]) || JS_ToInt32(ctx, &image, argv[5]) || JS_ToFloat64(ctx, &alpha, argv[6]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgImagePattern(g_NVGcontext, ox, oy, ex, ey, angle, image, alpha));
}

/*NVGJS_DECL(SetNextFillHoverable)
{
    nvgSetNextFillHoverable(g_NVGcontext);
    return JS_UNDEFINED;
}

NVGJS_DECL(IsFillHovered)
{
    int ret = nvgIsFillHovered(g_NVGcontext);
    return JS_NewBool(ctx, ret);
}

NVGJS_DECL(IsNextFillClicked)
{
    int ret = nvgIsNextFillClicked(g_NVGcontext);
    return JS_NewBool(ctx, ret);
}
*/

static const JSCFunctionListEntry nvgjs_funcs[] = {
#ifdef NANOVG_GL2
    NVGJS_FUNC(CreateGL2, 1),
#endif
#ifdef NANOVG_GL3
    NVGJS_FUNC(CreateGL3, 1),
#endif
    NVGJS_FUNC(CreateFont, 2),
    NVGJS_FUNC(CreateFontAtIndex, 3),
    NVGJS_FUNC(FindFont, 1),
    NVGJS_FUNC(BeginFrame, 3),
    NVGJS_FUNC(CancelFrame, 0),
    NVGJS_FUNC(EndFrame, 0),
    NVGJS_FUNC(Save, 0),
    NVGJS_FUNC(Restore, 0),
    NVGJS_FUNC(Reset, 0),
    NVGJS_FUNC(ShapeAntiAlias, 1),
    NVGJS_FUNC(ClosePath, 0),
    NVGJS_FUNC(Scissor, 4),
    NVGJS_FUNC(IntersectScissor, 4),
    NVGJS_FUNC(ResetScissor, 0),

    NVGJS_FUNC(MiterLimit, 1),
    NVGJS_FUNC(LineCap, 1),
    NVGJS_FUNC(LineJoin, 1),
    NVGJS_FUNC(GlobalAlpha, 1),
    NVGJS_FUNC(StrokeColor, 1),
    NVGJS_FUNC(StrokeWidth, 1),
    NVGJS_FUNC(StrokePaint, 1),
    NVGJS_FUNC(FillColor, 1),
    NVGJS_FUNC(FillPaint, 1),
    NVGJS_FUNC(LinearGradient, 6),
    NVGJS_FUNC(BoxGradient, 8),
    NVGJS_FUNC(RadialGradient, 6),
    NVGJS_FUNC(FontSize, 1),
    NVGJS_FUNC(FontBlur, 1),
    NVGJS_FUNC(TextLetterSpacing, 1),
    NVGJS_FUNC(TextLineHeight, 1),
    NVGJS_FUNC(TextAlign, 1),
    NVGJS_FUNC(FontFace, 1),
    NVGJS_FUNC(Text, 3),
    NVGJS_FUNC(TextBox, 4),
    NVGJS_FUNC(TextBounds, 5),
    NVGJS_FUNC(TextBoxBounds, 6),
    NVGJS_FUNC(TextBounds2, 3),
    NVGJS_FUNC(RGB, 3),
    NVGJS_FUNC(RGBf, 3),
    NVGJS_FUNC(RGBA, 4),
    NVGJS_FUNC(RGBAf, 4),
    NVGJS_FUNC(LerpRGBA, 3),
    NVGJS_FUNC(TransRGBA, 2),
    NVGJS_FUNC(TransRGBAf, 2),
    NVGJS_FUNC(HSL, 3),
    NVGJS_FUNC(HSLA, 4),
    NVGJS_FUNC(CreateImage, 2),
    NVGJS_FUNC(CreateImageMem, 2),
    NVGJS_FUNC(CreateImageRGBA, 4),
    NVGJS_FUNC(UpdateImage, 2),
    NVGJS_FUNC(ImageSize, 1),
    NVGJS_FUNC(DeleteImage, 1),
    NVGJS_FUNC(ResetTransform, 0),
    NVGJS_FUNC(Transform, 6),
    NVGJS_FUNC(Translate, 2),
    NVGJS_FUNC(Rotate, 1),
    NVGJS_FUNC(SkewX, 1),
    NVGJS_FUNC(SkewY, 1),
    NVGJS_FUNC(Scale, 2),
    NVGJS_FUNC(CurrentTransform, 1),
    NVGJS_FUNC(TransformIdentity, 0),
    NVGJS_FUNC(TransformTranslate, 2),
    NVGJS_FUNC(TransformScale, 2),
    NVGJS_FUNC(TransformRotate, 1),
    NVGJS_FUNC(TransformMultiply, 2),
    NVGJS_FUNC(TransformPremultiply, 2),
    NVGJS_FUNC(TransformInverse, 1),
    NVGJS_FUNC(TransformPoint, 2),
    NVGJS_FUNC(RadToDeg, 1),
    NVGJS_FUNC(DegToRad, 1),
    NVGJS_FUNC(ImagePattern, 7),
    NVGJS_FUNC(BeginPath, 0),
    NVGJS_FUNC(MoveTo, 2),
    NVGJS_FUNC(LineTo, 2),
    NVGJS_FUNC(BezierTo, 6),
    NVGJS_FUNC(QuadTo, 4),
    NVGJS_FUNC(ArcTo, 5),
    NVGJS_FUNC(Arc, 6),
    NVGJS_FUNC(Rect, 4),
    NVGJS_FUNC(Circle, 3),
    NVGJS_FUNC(Ellipse, 4),
    NVGJS_FUNC(RoundedRect, 5),
    NVGJS_FUNC(RoundedRectVarying, 8),
    NVGJS_FUNC(PathWinding, 1),
    NVGJS_FUNC(Stroke, 0),
    NVGJS_FUNC(Fill, 0),
    /*NVGJS_FUNC(SetNextFillHoverable, 0),
    NVGJS_FUNC(IsFillHovered, 0),
    NVGJS_FUNC(IsNextFillClicked, 0),*/
    NVGJS_FLAG(PI),
    NVGJS_FLAG(CCW),
    NVGJS_FLAG(CW),
    NVGJS_FLAG(SOLID),
    NVGJS_FLAG(HOLE),
    NVGJS_FLAG(BUTT),
    NVGJS_FLAG(ROUND),
    NVGJS_FLAG(SQUARE),
    NVGJS_FLAG(BEVEL),
    NVGJS_FLAG(MITER),
    NVGJS_FLAG(ALIGN_LEFT),
    NVGJS_FLAG(ALIGN_CENTER),
    NVGJS_FLAG(ALIGN_RIGHT),
    NVGJS_FLAG(ALIGN_TOP),
    NVGJS_FLAG(ALIGN_MIDDLE),
    NVGJS_FLAG(ALIGN_BOTTOM),
    NVGJS_FLAG(ALIGN_BASELINE),
    NVGJS_FLAG(ZERO),
    NVGJS_FLAG(ONE),
    NVGJS_FLAG(SRC_COLOR),
    NVGJS_FLAG(ONE_MINUS_SRC_COLOR),
    NVGJS_FLAG(DST_COLOR),
    NVGJS_FLAG(ONE_MINUS_DST_COLOR),
    NVGJS_FLAG(SRC_ALPHA),
    NVGJS_FLAG(ONE_MINUS_SRC_ALPHA),
    NVGJS_FLAG(DST_ALPHA),
    NVGJS_FLAG(ONE_MINUS_DST_ALPHA),
    NVGJS_FLAG(SRC_ALPHA_SATURATE),
    NVGJS_FLAG(SOURCE_OVER),
    NVGJS_FLAG(SOURCE_IN),
    NVGJS_FLAG(SOURCE_OUT),
    NVGJS_FLAG(ATOP),
    NVGJS_FLAG(DESTINATION_OVER),
    NVGJS_FLAG(DESTINATION_IN),
    NVGJS_FLAG(DESTINATION_OUT),
    NVGJS_FLAG(DESTINATION_ATOP),
    NVGJS_FLAG(LIGHTER),
    NVGJS_FLAG(COPY),
    NVGJS_FLAG(XOR),
    NVGJS_FLAG(IMAGE_GENERATE_MIPMAPS),
    NVGJS_FLAG(IMAGE_REPEATX),
    NVGJS_FLAG(IMAGE_REPEATY),
    NVGJS_FLAG(IMAGE_FLIPY),
    NVGJS_FLAG(IMAGE_PREMULTIPLIED),
    NVGJS_FLAG(IMAGE_NEAREST),
    NVGJS_FLAG(TEXTURE_ALPHA),
    NVGJS_FLAG(TEXTURE_RGBA),
};

static const JSCFunctionListEntry nvgjs_matrix_functions[] = {
    NVGJS_METHOD("identity", TransformIdentity, 0),
    NVGJS_METHOD("translate", TransformTranslate, 2),
    NVGJS_METHOD("scale", TransformScale, 2),
    NVGJS_METHOD("rotate", TransformRotate, 1),
    NVGJS_METHOD("skewX", TransformSkewX, 1),
    NVGJS_METHOD("skewY", TransformSkewY, 1),
    NVGJS_METHOD("multiply", TransformMultiply, 2),
    NVGJS_METHOD("premultiply", TransformPremultiply, 2),
    NVGJS_METHOD("inverse", TransformInverse, 1),
};

static int
nvgjs_init(JSContext* ctx, JSModuleDef* m) {
  JSValue paint_proto, paint_class;

  JSValue global = JS_GetGlobalObject(ctx);
  js_float32array_ctor = JS_GetPropertyStr(ctx, global, "Float32Array");
  js_float32array_proto = JS_GetPropertyStr(ctx, js_float32array_ctor, "prototype");
  JS_FreeValue(ctx, global);

  color_proto = JS_NewObjectProto(ctx, js_float32array_proto);
  JS_SetPropertyFunctionList(ctx, color_proto, nvgjs_color_methods, countof(nvgjs_color_methods));
  color_ctor = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetConstructor(ctx, color_ctor, color_proto);
  JS_SetModuleExport(ctx, m, "Color", color_ctor);

  matrix_proto = JS_NewObjectProto(ctx, js_float32array_proto);
  JS_SetPropertyFunctionList(ctx, matrix_proto, nvgjs_matrix_methods, countof(nvgjs_matrix_methods));
  matrix_ctor = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetPropertyFunctionList(ctx, matrix_ctor, nvgjs_matrix_functions, countof(nvgjs_matrix_functions));
  // JS_SetConstructor(ctx, matrix_ctor, matrix_proto);
  JS_SetModuleExport(ctx, m, "Matrix", matrix_ctor);

  JS_NewClassID(&nvgjs_paint_class_id);
  JS_NewClass(JS_GetRuntime(ctx), nvgjs_paint_class_id, &nvgjs_paint_class);

  paint_proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, paint_proto, nvgjs_paint_methods, countof(nvgjs_paint_methods));
  JS_SetClassProto(ctx, nvgjs_paint_class_id, paint_proto);
  paint_class = JS_NewObjectProto(ctx, JS_NULL);
  // JS_SetConstructor(ctx, paint_class, paint_proto);
  JS_SetModuleExport(ctx, m, "Paint", paint_class);

  JS_SetModuleExportList(ctx, m, nvgjs_funcs, countof(nvgjs_funcs));
  return 0;
}

VISIBLE JSModuleDef*
js_init_module_nanovg(JSContext* ctx, const char* module_name) {
  JSModuleDef* m;

  if(!(m = JS_NewCModule(ctx, module_name, nvgjs_init)))
    return NULL;

  JS_AddModuleExport(ctx, m, "Color");
  JS_AddModuleExport(ctx, m, "Matrix");
  JS_AddModuleExport(ctx, m, "Paint");
  JS_AddModuleExportList(ctx, m, nvgjs_funcs, countof(nvgjs_funcs));
  return m;
}

void
nvgjs_init_with_context(struct NVGcontext* vg) {
  g_NVGcontext = vg;
}
