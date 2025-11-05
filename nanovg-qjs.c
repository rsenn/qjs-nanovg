
#ifdef NANOVG_GLEW
#include <GL/glew.h>
#endif

#include "nanovg.h"
#include "nanovg_gl.h"

#include "nanovg-qjs.h"
#include "js-utils.h"

#include <assert.h>

static const char* const transform_obj_props[] = {"a", "b", "c", "d", "e", "f"};
static const int transform_arg_index[] = {0, 1, 2, 3, 4, 5};

static NVGcontext* g_NVGcontext;
static JSClassID /*js_nanovg_color_class_id,*/ js_nanovg_paint_class_id;

static JSValue js_float32array_ctor = JS_UNDEFINED, js_float32array_proto = JS_UNDEFINED;
static JSValue color_ctor = JS_UNDEFINED, color_proto = JS_UNDEFINED;
static JSValue matrix_ctor = JS_UNDEFINED, matrix_proto = JS_UNDEFINED;

static int
js_nanovg_tocolor(JSContext* ctx, NVGcolor* color, JSValueConst value) {
  size_t offset = 0, length = 0, bytes_per_element = 0;
  JSValue buf = JS_GetTypedArrayBuffer(ctx, value, &offset, &length, &bytes_per_element);

  if(!JS_IsException(buf) && bytes_per_element == sizeof(float) && length >= 4) {
    size_t len;
    uint8_t* ptr = JS_GetArrayBuffer(ctx, &len, buf);
    JS_FreeValue(ctx, buf);

    if(ptr) {
      memcpy(color, ptr + offset, sizeof(*color));
      return 0;
    }
  }

  JSValue iter = js_iterator_new(ctx, value);
  int ret = 0;

  if(JS_IsObject(iter)) {
    for(int i = 0; i < 4; i++) {
      BOOL done = FALSE;
      JSValue val = js_iterator_next(ctx, iter, &done);

      if(!done)
        ret |= js_tofloat32(ctx, &color->rgba[i], val);

      JS_FreeValue(ctx, val);

      if(done) {
        if(i < 3) {
          ret = 1;
          break;
        }

        color->a = 1.0;
      }
    }

    JS_FreeValue(ctx, iter);
  } else if(JS_IsObject(value)) {
    ret |= js_get_property_str_float32(ctx, value, "r", &color->r);
    ret |= js_get_property_str_float32(ctx, value, "g", &color->g);
    ret |= js_get_property_str_float32(ctx, value, "b", &color->b);
    ret |= js_get_property_str_float32(ctx, value, "a", &color->a);
  }

  return ret;
}

static JSValue
js_nanovg_color_new(JSContext* ctx, NVGcolor color) {
  JSValue buf = JS_NewArrayBufferCopy(ctx, (const void*)&color, sizeof(color));
  JSValue obj = JS_CallConstructor(ctx, js_float32array_ctor, 1, &buf);
  JS_FreeValue(ctx, buf);
  JS_SetPrototype(ctx, obj, color_proto);

  return obj;
}

static JSValue
js_nanovg_color_get(JSContext* ctx, JSValueConst this_val, int magic) {
  return JS_GetPropertyUint32(ctx, this_val, magic);
}

static JSValue
js_nanovg_color_set(JSContext* ctx, JSValueConst this_val, JSValueConst value, int magic) {
  JS_SetPropertyUint32(ctx, this_val, magic, JS_DupValue(ctx, value));
  return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_nanovg_color_methods[] = {
    JS_CGETSET_MAGIC_DEF("r", js_nanovg_color_get, js_nanovg_color_set, 0),
    JS_CGETSET_MAGIC_DEF("g", js_nanovg_color_get, js_nanovg_color_set, 1),
    JS_CGETSET_MAGIC_DEF("b", js_nanovg_color_get, js_nanovg_color_set, 2),
    JS_CGETSET_MAGIC_DEF("a", js_nanovg_color_get, js_nanovg_color_set, 3),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "nvgColor", JS_PROP_CONFIGURABLE),
};

static JSValue
js_nanovg_matrix_new(JSContext* ctx, float matrix[6]) {
  JSValue buf = JS_NewArrayBufferCopy(ctx, (const void*)matrix, sizeof(*matrix));
  JSValue obj = JS_CallConstructor(ctx, js_float32array_ctor, 1, &buf);
  JS_FreeValue(ctx, buf);
  JS_SetPrototype(ctx, obj, matrix_proto);

  return obj;
}

static JSValue
js_nanovg_matrix_get(JSContext* ctx, JSValueConst this_val, int magic) {
  return JS_GetPropertyUint32(ctx, this_val, magic);
}

static JSValue
js_nanovg_matrix_set(JSContext* ctx, JSValueConst this_val, JSValueConst value, int magic) {
  JS_SetPropertyUint32(ctx, this_val, magic, JS_DupValue(ctx, value));
  return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_nanovg_matrix_methods[] = {
    JS_CGETSET_MAGIC_DEF("a", js_nanovg_matrix_get, js_nanovg_matrix_set, 0),
    JS_CGETSET_MAGIC_DEF("b", js_nanovg_matrix_get, js_nanovg_matrix_set, 1),
    JS_CGETSET_MAGIC_DEF("c", js_nanovg_matrix_get, js_nanovg_matrix_set, 2),
    JS_CGETSET_MAGIC_DEF("d", js_nanovg_matrix_get, js_nanovg_matrix_set, 3),
    JS_CGETSET_MAGIC_DEF("e", js_nanovg_matrix_get, js_nanovg_matrix_set, 4),
    JS_CGETSET_MAGIC_DEF("f", js_nanovg_matrix_get, js_nanovg_matrix_set, 5),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "nvgMatrix", JS_PROP_CONFIGURABLE),
};

static void
js_nanovg_paint_finalizer(JSRuntime* rt, JSValue val) {
  NVGpaint* p;

  if((p = JS_GetOpaque(val, js_nanovg_paint_class_id))) {
    js_free_rt(rt, p);
  }
}

static JSValue
js_nanovg_paint_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  NVGpaint* p;
  JSValue proto, obj = JS_UNDEFINED;

  if(!(p = js_mallocz(ctx, sizeof(*p))))
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
js_get_transform(JSContext* ctx, JSValueConst this_obj, float* xform) {
  int ret = 0;

  if(JS_IsArray(ctx, this_obj)) {
    for(int i = 0; i < 6; i++)
      if(ret = js_get_property_uint_float32(ctx, this_obj, i, &xform[i]))
        break;
  } else if(JS_IsObject(this_obj)) {
    for(int i = 0; i < 6; i++)
      if(ret = js_get_property_str_float32(ctx, this_obj, transform_obj_props[i], &xform[i]))
        break;
  } else {
    ret = 1;
  }

  return ret;
}

void
js_set_transform(JSContext* ctx, JSValueConst this_obj, const float* xform) {
  if(JS_IsArray(ctx, this_obj))
    for(int i = 0; i < 6; i++)
      JS_SetPropertyUint32(ctx, this_obj, i, JS_NewFloat64(ctx, xform[i]));
  else
    for(int i = 0; i < 6; i++)
      JS_SetPropertyStr(ctx, this_obj, transform_obj_props[i], JS_NewFloat64(ctx, xform[i]));
}

JSValue
js_new_transform(JSContext* ctx, const float* xform) {
  JSValue ret = JS_NewArray(ctx);
  js_set_transform(ctx, ret, xform);
  return ret;
}

int
js_argument_transform(JSContext* ctx, float* xform, int argc, JSValueConst argv[]) {
  int i = 0;

  if(argc >= 6)
    for(i = 0; i < 6; i++)
      if(js_tofloat32(ctx, &xform[transform_arg_index[i]], argv[i]))
        break;

  if(i == 6)
    return i;

  return !js_get_transform(ctx, argv[0], xform);
}

int
js_get_vector(JSContext* ctx, JSValueConst this_obj, float vec[2]) {
  int ret = 0;

  if(JS_IsArray(ctx, this_obj)) {
    for(int i = 0; i < 2; i++)
      if((ret = js_get_property_uint_float32(ctx, this_obj, i, &vec[i])))
        break;
  } else if(JS_IsObject(this_obj)) {
    for(int i = 0; i < 2; i++)
      if((ret = js_get_property_str_float32(ctx, this_obj, i ? "y" : "x", &vec[i])))
        break;
  } else {
    ret = 1;
  }

  return ret;
}

void
js_set_vector(JSContext* ctx, JSValueConst this_obj, const float vec[2]) {
  if(JS_IsArray(ctx, this_obj)) {
    JS_SetPropertyUint32(ctx, this_obj, 0, JS_NewFloat64(ctx, vec[0]));
    JS_SetPropertyUint32(ctx, this_obj, 1, JS_NewFloat64(ctx, vec[1]));
  } else {
    JS_SetPropertyStr(ctx, this_obj, "x", JS_NewFloat64(ctx, vec[0]));
    JS_SetPropertyStr(ctx, this_obj, "y", JS_NewFloat64(ctx, vec[1]));
  }
}

int
js_argument_vector(JSContext* ctx, float vec[2], int argc, JSValueConst argv[]) {
  int i = 0;

  if(argc >= 2)
    for(i = 0; i < 2; i++)
      if(js_tofloat32(ctx, &vec[i], argv[i]))
        break;

  if(i == 2)
    return i;

  return !js_get_vector(ctx, argv[0], vec);
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

FUNC(DeleteGL3) {
  if(g_NVGcontext) {
    nvgDeleteGL3(g_NVGcontext);
    g_NVGcontext = 0;
  }

  return JS_UNDEFINED;
}
#endif

FUNC(CreateFont) {
  const char* name = JS_ToCString(ctx, argv[0]);
  const char* file = JS_ToCString(ctx, argv[1]);

  return JS_NewInt32(ctx, nvgCreateFont(g_NVGcontext, name, file));
}

FUNC(BeginFrame) {
  double w, h, ratio;

  if(JS_ToFloat64(ctx, &w, argv[0]) || JS_ToFloat64(ctx, &h, argv[1]) || JS_ToFloat64(ctx, &ratio, argv[2]))
    return JS_EXCEPTION;

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
  nvgRestore(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(Reset) {
  nvgReset(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(ShapeAntiAlias) {
  nvgShapeAntiAlias(g_NVGcontext, JS_ToBool(ctx, argv[0]));
  return JS_UNDEFINED;
}

FUNC(Rect) {
  double x, y, w, h;

  if(argc != 4)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgRect(g_NVGcontext, x, y, w, h);
  return JS_UNDEFINED;
}

FUNC(Circle) {
  double cx, cy, r;

  if(argc != 3)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &r, argv[2]))
    return JS_EXCEPTION;

  nvgCircle(g_NVGcontext, cx, cy, r);
  return JS_UNDEFINED;
}

FUNC(Ellipse) {
  double cx, cy, rx, ry;

  if(argc != 4)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &rx, argv[2]) || JS_ToFloat64(ctx, &ry, argv[3]))
    return JS_EXCEPTION;

  nvgEllipse(g_NVGcontext, cx, cy, rx, ry);
  return JS_UNDEFINED;
}

FUNC(PathWinding) {
  int dir;

  if(argc != 1)
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &dir, argv[0]))
    return JS_EXCEPTION;

  nvgPathWinding(g_NVGcontext, dir);
  return JS_UNDEFINED;
}

FUNC(MoveTo) {
  double x, y;

  if(argc != 2)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgMoveTo(g_NVGcontext, x, y);
  return JS_UNDEFINED;
}

FUNC(LineTo) {
  double x, y;

  if(argc != 2)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgLineTo(g_NVGcontext, x, y);
  return JS_UNDEFINED;
}

FUNC(ClosePath) {
  nvgClosePath(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(FontBlur) {
  double blur;

  if(argc != 1)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &blur, argv[0]))
    return JS_EXCEPTION;

  nvgFontBlur(g_NVGcontext, blur);
  return JS_UNDEFINED;
}

FUNC(BeginPath) {
  nvgBeginPath(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(RoundedRect) {
  double x, y, w, h, r;

  if(argc != 5)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &r, argv[4]))
    return JS_EXCEPTION;

  nvgRoundedRect(g_NVGcontext, x, y, w, h, r);
  return JS_UNDEFINED;
}

FUNC(Scissor) {
  double x, y, w, h;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgScissor(g_NVGcontext, x, y, w, h);
  return JS_UNDEFINED;
}

FUNC(IntersectScissor) {
  double x, y, w, h;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgIntersectScissor(g_NVGcontext, x, y, w, h);

  return JS_UNDEFINED;
}

FUNC(ResetScissor) {
  nvgResetScissor(g_NVGcontext);
}

FUNC(FillPaint) {
  NVGpaint* paint;

  if(argc != 1)
    return JS_EXCEPTION;

  if(!(paint = JS_GetOpaque2(ctx, argv[0], js_nanovg_paint_class_id)))
    return JS_EXCEPTION;

  nvgFillPaint(g_NVGcontext, *paint);
  return JS_UNDEFINED;
}

FUNC(Fill) {
  nvgFill(g_NVGcontext);
  return JS_UNDEFINED;
}

FUNC(MiterLimit) {
  double miterLimit;

  if(JS_ToFloat64(ctx, &miterLimit, argv[0]))
    return JS_EXCEPTION;

  nvgMiterLimit(g_NVGcontext, miterLimit);
  return JS_UNDEFINED;
}

FUNC(LineCap) {
  int32_t lineCap;

  if(JS_ToInt32(ctx, &lineCap, argv[0]))
    return JS_EXCEPTION;

  nvgLineCap(g_NVGcontext, lineCap);
  return JS_UNDEFINED;
}

FUNC(LineJoin) {
  int32_t lineJoin;

  if(JS_ToInt32(ctx, &lineJoin, argv[0]))
    return JS_EXCEPTION;

  nvgLineCap(g_NVGcontext, lineJoin);
  return JS_UNDEFINED;
}

FUNC(GlobalAlpha) {
  double alpha;

  if(JS_ToFloat64(ctx, &alpha, argv[0]))
    return JS_EXCEPTION;

  nvgGlobalAlpha(g_NVGcontext, alpha);
  return JS_UNDEFINED;
}

FUNC(StrokeColor) {
  NVGcolor color;

  if(argc != 1)
    return JS_EXCEPTION;

  if(js_nanovg_tocolor(ctx, &color, argv[0]))
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
  if(js_nanovg_tocolor(ctx, &color, argv[0]))
    return JS_EXCEPTION;

  nvgFillColor(g_NVGcontext, color);
  return JS_UNDEFINED;
}

FUNC(LinearGradient) {
  double sx, sy, ex, ey;
  NVGcolor icol, ocol;

  if(argc != 6)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &sx, argv[0]) || JS_ToFloat64(ctx, &sy, argv[1]) || JS_ToFloat64(ctx, &ex, argv[2]) || JS_ToFloat64(ctx, &ey, argv[3]) || js_nanovg_tocolor(ctx, &icol, argv[4]) || js_nanovg_tocolor(ctx, &ocol, argv[5]))
    return JS_EXCEPTION;

  NVGpaint paint = nvgLinearGradient(g_NVGcontext, sx, sy, ex, ey, icol, ocol);
  NVGpaint* p = js_mallocz(ctx, sizeof(NVGpaint));
  *p = paint;

  return js_nanovg_wrap(ctx, p, js_nanovg_paint_class_id);
}

FUNC(BoxGradient) {
  double x, y, w, h, r, f;
  NVGcolor icol, ocol;

  if(argc != 8)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &r, argv[4]) || JS_ToFloat64(ctx, &f, argv[5]) || js_nanovg_tocolor(ctx, &icol, argv[6]) || js_nanovg_tocolor(ctx, &ocol, argv[7]))
    return JS_EXCEPTION;

  NVGpaint paint = nvgBoxGradient(g_NVGcontext, x, y, w, h, r, f, icol, ocol);
  NVGpaint* p = js_mallocz(ctx, sizeof(NVGpaint));
  *p = paint;
  return js_nanovg_wrap(ctx, p, js_nanovg_paint_class_id);
}

FUNC(RadialGradient) {
  double cx, cy, inr, outr;
  NVGcolor icol, ocol;

  if(argc != 6)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &inr, argv[2]) || JS_ToFloat64(ctx, &outr, argv[3]) || js_nanovg_tocolor(ctx, &icol, argv[4]) || js_nanovg_tocolor(ctx, &ocol, argv[5]))
    return JS_EXCEPTION;

  NVGpaint paint = nvgRadialGradient(g_NVGcontext, cx, cy, inr, outr, icol, ocol);
  NVGpaint* p = js_mallocz(ctx, sizeof(NVGpaint));
  *p = paint;
  return js_nanovg_wrap(ctx, p, js_nanovg_paint_class_id);
}

FUNC(TextBounds) {
  double x, y;
  const char* str;

  if(argc != 3)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  if(!(str = JS_ToCString(ctx, argv[2])))
    return JS_EXCEPTION;

  float ret = nvgTextBounds(g_NVGcontext, x, y, str, NULL, NULL);

  JS_FreeCString(ctx, str);

  return JS_NewFloat64(ctx, ret);
}

FUNC(TextBounds2) {
  double x, y;
  const char* str;

  if(argc != 3)
    return JS_EXCEPTION;

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

FUNC(FontSize) {
  double size;

  if(argc != 1)
    return JS_EXCEPTION;

  if(JS_ToFloat64(ctx, &size, argv[0]))
    return JS_EXCEPTION;

  nvgFontSize(g_NVGcontext, size);
  return JS_UNDEFINED;
}

FUNC(FontFace) {
  const char* str;

  if(argc != 1)
    return JS_EXCEPTION;

  if(!(str = JS_ToCString(ctx, argv[0])))
    return JS_EXCEPTION;

  nvgFontFace(g_NVGcontext, str);
  JS_FreeCString(ctx, str);
  return JS_UNDEFINED;
}

FUNC(TextAlign) {
  int align;

  if(argc != 1)
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &align, argv[0]))
    return JS_EXCEPTION;

  nvgTextAlign(g_NVGcontext, align);
  return JS_UNDEFINED;
}

FUNC(Text) {
  int x, y;
  const char* str;

  if(argc != 3)
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &x, argv[0]) || JS_ToInt32(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  if(!(str = JS_ToCString(ctx, argv[2])))
    return JS_EXCEPTION;

  float ret = nvgText(g_NVGcontext, x, y, str, NULL);

  JS_FreeCString(ctx, str);

  return JS_NewFloat64(ctx, ret);
}

FUNC(RGB) {
  int32_t r, g, b;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToInt32(ctx, &r, argv[0]) || JS_ToInt32(ctx, &g, argv[1]) || JS_ToInt32(ctx, &b, argv[2]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgRGB(r, g, b));
}

FUNC(RGBf) {
  double r, g, b;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &r, argv[0]) || JS_ToFloat64(ctx, &g, argv[1]) || JS_ToFloat64(ctx, &b, argv[2]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgRGBf(r, g, b));
}

FUNC(RGBA) {
  int32_t r, g, b, a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToInt32(ctx, &r, argv[0]) || JS_ToInt32(ctx, &g, argv[1]) || JS_ToInt32(ctx, &b, argv[2]) || JS_ToInt32(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgRGBA(r, g, b, a));
}

FUNC(RGBAf) {
  double r, g, b, a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &r, argv[0]) || JS_ToFloat64(ctx, &g, argv[1]) || JS_ToFloat64(ctx, &b, argv[2]) || JS_ToFloat64(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgRGBAf(r, g, b, a));
}

FUNC(LerpRGBA) {
  NVGcolor c0, c1;
  double u;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(js_nanovg_tocolor(ctx, &c0, argv[0]) || js_nanovg_tocolor(ctx, &c1, argv[1]) || JS_ToFloat64(ctx, &u, argv[2]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgLerpRGBA(c0, c1, u));
}

FUNC(TransRGBA) {
  NVGcolor c;
  int32_t a;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(js_nanovg_tocolor(ctx, &c, argv[0]) || JS_ToInt32(ctx, &a, argv[1]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgTransRGBA(c, a));
}

FUNC(TransRGBAf) {
  NVGcolor c;
  double a;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(js_nanovg_tocolor(ctx, &c, argv[0]) || JS_ToFloat64(ctx, &a, argv[1]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgTransRGBAf(c, a));
}

FUNC(HSL) {
  double h, s, l;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &h, argv[0]) || JS_ToFloat64(ctx, &s, argv[1]) || JS_ToFloat64(ctx, &l, argv[2]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgHSL(h, s, l));
}

FUNC(HSLA) {
  double h, s, l;
  int32_t a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &h, argv[0]) || JS_ToFloat64(ctx, &s, argv[1]) || JS_ToFloat64(ctx, &l, argv[2]) || JS_ToInt32(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return js_nanovg_color_new(ctx, nvgHSLA(h, s, l, a));
}

FUNC(StrokeWidth) {
  double width;

  if(JS_ToFloat64(ctx, &width, argv[0]))
    return JS_EXCEPTION;

  nvgStrokeWidth(g_NVGcontext, width);
  return JS_UNDEFINED;
}

FUNC(CreateImage) {
  const char* file = JS_ToCString(ctx, argv[0]);
  int32_t flags = 0;

  if(JS_ToInt32(ctx, &flags, argv[1]))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateImage(g_NVGcontext, file, flags));
}

FUNC(CreateImageRGBA) {
  const char* file = JS_ToCString(ctx, argv[0]);
  int32_t width, height, flags;
  uint8_t* ptr;
  size_t len;

  if(JS_ToInt32(ctx, &width, argv[0]) || JS_ToInt32(ctx, &height, argv[1]) || JS_ToInt32(ctx, &flags, argv[2]))
    return JS_EXCEPTION;

  if(!(ptr = JS_GetArrayBuffer(ctx, &len, argv[3])))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateImageRGBA(g_NVGcontext, width, height, flags, (void*)ptr));
}

FUNC(UpdateImage) {
  int32_t image;
  size_t len;
  uint8_t* ptr;

  if(JS_ToInt32(ctx, &image, argv[0]))
    return JS_EXCEPTION;

  if(!(ptr = JS_GetArrayBuffer(ctx, &len, argv[1])))
    return JS_EXCEPTION;

  nvgUpdateImage(g_NVGcontext, image, (void*)ptr);

  return JS_UNDEFINED;
}

FUNC(ImageSize) {
  int32_t id = 0;
  int width, height;
  JSValue ret;

  if(JS_ToInt32(ctx, &id, argv[0]))
    return JS_EXCEPTION;

  nvgImageSize(g_NVGcontext, id, &width, &height);

  ret = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, ret, 0, JS_NewInt32(ctx, width));
  JS_SetPropertyUint32(ctx, ret, 1, JS_NewInt32(ctx, height));
  return ret;
}

FUNC(DeleteImage) {
  int32_t id = 0;

  if(JS_ToInt32(ctx, &id, argv[0]))
    return JS_EXCEPTION;

  nvgDeleteImage(g_NVGcontext, id);

  return JS_UNDEFINED;
}

FUNC(ResetTransform) {
  nvgResetTransform(g_NVGcontext);

  return JS_UNDEFINED;
}

FUNC(Transform) {
  float t[6];
  int n;

  while((n = js_argument_transform(ctx, t, argc, argv))) {
    nvgTransform(g_NVGcontext, t[transform_arg_index[0]], t[transform_arg_index[1]], t[transform_arg_index[2]], t[transform_arg_index[3]], t[transform_arg_index[4]], t[transform_arg_index[5]]);

    argc -= n;
    argv += n;
  }

  return JS_UNDEFINED;
}

FUNC(Translate) {
  double x, y;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgTranslate(g_NVGcontext, x, y);

  return JS_UNDEFINED;
}

FUNC(Rotate) {
  double angle;

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgRotate(g_NVGcontext, angle);

  return JS_UNDEFINED;
}

FUNC(SkewX) {
  double angle;

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgSkewX(g_NVGcontext, angle);

  return JS_UNDEFINED;
}

FUNC(SkewY) {
  double angle;

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgSkewY(g_NVGcontext, angle);

  return JS_UNDEFINED;
}

FUNC(Scale) {
  double x, y;

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgScale(g_NVGcontext, x, y);

  return JS_UNDEFINED;
}

FUNC(CurrentTransform) {
  float t[6];

  nvgCurrentTransform(g_NVGcontext, t);
  js_set_transform(ctx, argv[0], t);

  return JS_UNDEFINED;
}

FUNC(TransformIdentity) {
  float t[6];

  nvgTransformIdentity(t);

  if(argc == 0)
    return js_new_transform(ctx, t);

  js_set_transform(ctx, argv[0], t);
  return JS_UNDEFINED;
}

FUNC(TransformTranslate) {
  float t[6];
  int32_t x, y;
  int i = 0;

  if(argc >= 3 && JS_IsObject(argv[0]))
    i++;

  if(JS_ToInt32(ctx, &x, argv[i]) || JS_ToInt32(ctx, &y, argv[i + 1]))
    return JS_EXCEPTION;

  nvgTransformTranslate(t, x, y);

  if(i == 0)
    return js_new_transform(ctx, t);

  js_set_transform(ctx, argv[0], t);
  return JS_UNDEFINED;
}

FUNC(TransformScale) {
  float t[6];
  double x, y;
  int i = 0;

  if(argc >= 3 && JS_IsObject(argv[0]))
    i++;

  if(JS_ToFloat64(ctx, &x, argv[i]) || JS_ToFloat64(ctx, &y, argv[i + 1]))
    return JS_EXCEPTION;

  nvgTransformScale(t, x, y);

  if(i == 0)
    return js_new_transform(ctx, t);

  js_set_transform(ctx, argv[0], t);
  return JS_UNDEFINED;
}

FUNC(TransformRotate) {
  float t[6];
  double angle;
  int i = 0;

  if(argc >= 2 && JS_IsObject(argv[0]))
    i++;

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformRotate(t, angle);

  if(i == 0)
    return js_new_transform(ctx, t);

  js_set_transform(ctx, argv[0], t);
  return JS_UNDEFINED;
}

FUNC(TransformMultiply) {
  float dst[6], src[6];
  int i = 1, n;

  js_get_transform(ctx, argv[0], dst);

  while(i < argc && (n = js_argument_transform(ctx, src, argc - i, argv + i))) {
    nvgTransformMultiply(dst, src);
    i += n;
  }

  js_set_transform(ctx, argv[0], dst);
  return JS_UNDEFINED;
}

FUNC(TransformPremultiply) {
  float dst[6], src[6];
  int i = 1, n;

  js_get_transform(ctx, argv[0], dst);

  while(i < argc && (n = js_argument_transform(ctx, src, argc - i, argv + i))) {
    nvgTransformPremultiply(dst, src);
    i += n;
  }

  js_set_transform(ctx, argv[0], dst);
  return JS_UNDEFINED;
}

FUNC(TransformInverse) {
  float dst[6], src[6];
  int i = 0;

  if(argc > 1)
    i++;

  js_get_transform(ctx, argv[i], src);

  int ret = nvgTransformInverse(dst, src);

  if(ret) {
    if(argc == 1)
      return js_new_transform(ctx, dst);
    js_set_transform(ctx, argv[0], dst);
  }

  return JS_NewInt32(ctx, ret);
}

FUNC(TransformPoint) {
  float m[6], dst[2], src[2];

  js_get_vector(ctx, argv[0], dst);
  js_get_transform(ctx, argv[1], m);
  js_argument_vector(ctx, src, argc - 2, argv + 2);
  nvgTransformPoint(&dst[0], &dst[1], m, src[0], src[1]);
  js_set_vector(ctx, argv[0], dst);

  return JS_UNDEFINED;
}

FUNC(DegToRad) {
  double arg;

  if(JS_ToFloat64(ctx, &arg, argv[0]))
    return JS_EXCEPTION;

  return JS_NewFloat64(ctx, nvgDegToRad(arg));
}

FUNC(RadToDeg) {
  double arg;

  if(JS_ToFloat64(ctx, &arg, argv[0]))
    return JS_EXCEPTION;

  return JS_NewFloat64(ctx, nvgRadToDeg(arg));
}

FUNC(ImagePattern) {
  double ox, oy, ex, ey;
  double angle, alpha;
  int32_t image;
  NVGpaint paint, *p;

  if(JS_ToFloat64(ctx, &ox, argv[0]) || JS_ToFloat64(ctx, &oy, argv[1]) || JS_ToFloat64(ctx, &ex, argv[2]) || JS_ToFloat64(ctx, &ey, argv[3]) || JS_ToFloat64(ctx, &angle, argv[4]) || JS_ToInt32(ctx, &image, argv[5]) || JS_ToFloat64(ctx, &alpha, argv[6]))
    return JS_EXCEPTION;

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
    _JS_CFUNC_DEF(CreateFont, 2),
    _JS_CFUNC_DEF(BeginFrame, 3),
    _JS_CFUNC_DEF(CancelFrame, 0),
    _JS_CFUNC_DEF(EndFrame, 0),
    _JS_CFUNC_DEF(Save, 0),
    _JS_CFUNC_DEF(Restore, 0),
    _JS_CFUNC_DEF(Reset, 0),
    _JS_CFUNC_DEF(ShapeAntiAlias, 1),
    _JS_CFUNC_DEF(ClosePath, 0),
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
    _JS_CFUNC_DEF(LerpRGBA, 3),
    _JS_CFUNC_DEF(TransRGBA, 2),
    _JS_CFUNC_DEF(TransRGBAf, 2),
    _JS_CFUNC_DEF(HSL, 3),
    _JS_CFUNC_DEF(HSLA, 4),
    _JS_CFUNC_DEF(CreateImage, 2),
    _JS_CFUNC_DEF(CreateImageRGBA, 4),
    _JS_CFUNC_DEF(UpdateImage, 2),
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
    _JS_CFUNC_DEF(TransformIdentity, 0),
    _JS_CFUNC_DEF(TransformTranslate, 2),
    _JS_CFUNC_DEF(TransformScale, 2),
    _JS_CFUNC_DEF(TransformRotate, 1),
    _JS_CFUNC_DEF(TransformMultiply, 2),
    _JS_CFUNC_DEF(TransformPremultiply, 2),
    _JS_CFUNC_DEF(TransformInverse, 1),
    _JS_CFUNC_DEF(TransformPoint, 2),
    _JS_CFUNC_DEF(RadToDeg, 1),
    _JS_CFUNC_DEF(DegToRad, 1),
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
    /*_JS_CFUNC_DEF(SetNextFillHoverable, 0),
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
    _JS_NANOVG_FLAG(TEXTURE_ALPHA),
    _JS_NANOVG_FLAG(TEXTURE_RGBA),
};

static int
js_nanovg_init(JSContext* ctx, JSModuleDef* m) {
  JSValue paint_proto, paint_class;

  JSValue global = JS_GetGlobalObject(ctx);
  js_float32array_ctor = JS_GetPropertyStr(ctx, global, "Float32Array");
  js_float32array_proto = JS_GetPropertyStr(ctx, js_float32array_ctor, "prototype");
  JS_FreeValue(ctx, global);

  // JS_NewClassID(&js_nanovg_color_class_id);

  color_proto = JS_NewObjectProto(ctx, js_float32array_proto);
  JS_SetPropertyFunctionList(ctx, color_proto, js_nanovg_color_methods, countof(js_nanovg_color_methods));
  // JS_SetClassProto(ctx, js_nanovg_color_class_id, color_proto);
  color_ctor = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetConstructor(ctx, color_ctor, color_proto);

  JS_SetModuleExport(ctx, m, "Color", color_ctor);

  // JS_NewClassID(&js_nanovg_matrix_class_id);

  matrix_proto = JS_NewObjectProto(ctx, js_float32array_proto);
  JS_SetPropertyFunctionList(ctx, matrix_proto, js_nanovg_matrix_methods, countof(js_nanovg_matrix_methods));
  // JS_SetClassProto(ctx, js_nanovg_matrix_class_id, matrix_proto);
  matrix_ctor = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetConstructor(ctx, matrix_ctor, matrix_proto);

  JS_SetModuleExport(ctx, m, "Matrix", matrix_ctor);

  JS_NewClassID(&js_nanovg_paint_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_nanovg_paint_class_id, &js_nanovg_paint_class);

  paint_proto = JS_NewObject(ctx);
  JS_SetClassProto(ctx, js_nanovg_paint_class_id, paint_proto);
  paint_class = JS_NewCFunction2(ctx, js_nanovg_paint_constructor, "Paint", 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor(ctx, paint_class, paint_proto);

  JS_SetModuleExport(ctx, m, "Paint", paint_class);
  JS_SetModuleExportList(ctx, m, js_nanovg_funcs, countof(js_nanovg_funcs));
  return 0;
}

static JSModuleDef*
js_init_module_nanovg(JSContext* ctx, const char* module_name) {
  JSModuleDef* m;

  if(!(m = JS_NewCModule(ctx, module_name, js_nanovg_init)))
    return NULL;

  JS_AddModuleExport(ctx, m, "Color");
  JS_AddModuleExport(ctx, m, "Matrix");
  JS_AddModuleExport(ctx, m, "Paint");
  JS_AddModuleExportList(ctx, m, js_nanovg_funcs, countof(js_nanovg_funcs));
  return m;
}

#ifdef JS_SHARED_LIBRARY
VISIBLE JSModuleDef*
js_init_module(JSContext* ctx, const char* module_name) {
  return js_init_module_nanovg(ctx, module_name);
}
#endif

void
js_nanovg_init_with_context(struct NVGcontext* vg) {
  g_NVGcontext = vg;
}
