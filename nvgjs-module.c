#ifdef NANOVG_GLEW
#include <GL/glew.h>
#endif

#include "nanovg.h"
#include "nanovg_gl.h"
#include "nanovg_gl_utils.h"

#include "nvgjs-module.h"
#include "nvgjs-utils.h"

#include <assert.h>

static JSClassID nvgjs_context_class_id, nvgjs_paint_class_id, nvgjs_framebuffer_class_id;

static JSValue js_float32array_ctor = JS_UNDEFINED, js_float32array_proto = JS_UNDEFINED;
static JSValue color_ctor = JS_UNDEFINED, color_proto = JS_UNDEFINED;
static JSValue transform_ctor = JS_UNDEFINED, transform_proto = JS_UNDEFINED;
static JSValue context_ctor = JS_UNDEFINED, context_proto = JS_UNDEFINED;
static JSValue framebuffer_ctor = JS_UNDEFINED, framebuffer_proto = JS_UNDEFINED;

static JSValue nvgjs_framebuffer_wrap(JSContext*, JSValueConst, NVGLUframebuffer*);

static float*
nvgjs_output(JSContext* ctx, size_t min_size, JSValueConst value) {
  size_t length;
  float* ptr;

  if(nvgjs_same_object(value, transform_ctor))
    return 0;

  if((ptr = nvgjs_outputarray(ctx, &length, value)))
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
  return nvgjs_inputoutputarray(ctx, color->rgba, 4, value);
}

static JSValue
nvgjs_wrap(JSContext* ctx, void* s, JSClassID classID) {
  JSValue obj = JS_NewObjectClass(ctx, classID);

  if(JS_IsException(obj))
    return obj;

  JS_SetOpaque(obj, s);
  return obj;
}

static const char* const nvgjs_transform_keys[] = {"a", "b", "c", "d", "e", "f"};

static float*
nvgjs_transform_output(JSContext* ctx, JSValueConst value) {
  if(nvgjs_same_object(value, transform_ctor))
    return 0;

  return nvgjs_output(ctx, 6, value);
}

static JSValue
nvgjs_transform_copy(JSContext* ctx, JSValueConst value, const float transform[6]) {
  assert(JS_IsObject(value));
  assert(!nvgjs_same_object(value, transform_ctor));

  nvgjs_copyarray(ctx, value, transform, 6);
  return JS_UNDEFINED;
}

static int
nvgjs_transform_arguments(JSContext* ctx, float transform[6], int argc, JSValueConst argv[]) {
  int i = 0;

  if(argc >= 6)
    for(i = 0; i < 6; i++)
      if(nvgjs_tofloat32(ctx, &transform[i], argv[i]))
        break;

  if(i == 6)
    return i;

  return !nvgjs_inputoutputarray(ctx, transform, 6, argv[0]);
}

static const char* const nvgjs_vector_keys[] = {"x", "y"};

static int
nvgjs_tovector(JSContext* ctx, float vec[2], JSValueConst value) {
  return nvgjs_inputoutputarray(ctx, vec, 2, value);
}

static int
nvgjs_vector_copy(JSContext* ctx, JSValueConst value, const float vec[2]) {
  if(JS_IsArray(ctx, value))
    return nvgjs_copyarray(ctx, value, vec, 2);

  return nvgjs_copyobject(ctx, value, nvgjs_vector_keys, vec, 2);
}

static int
nvgjs_vector_arguments(JSContext* ctx, float vec[2], int argc, JSValueConst argv[]) {
  int i = 0;

  if(argc >= 2)
    for(i = 0; i < 2; i++)
      if(nvgjs_tofloat32(ctx, &vec[i], argv[i]))
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
nvgjs_transform_new(JSContext* ctx, float transform[6]) {
  JSValue buf = JS_NewArrayBufferCopy(ctx, (void*)transform, 6 * sizeof(float));
  JSValue obj = JS_CallConstructor(ctx, js_float32array_ctor, 1, &buf);
  JS_FreeValue(ctx, buf);
  JS_SetPrototype(ctx, obj, transform_proto);

  return obj;
}

static JSValue
nvgjs_transform_get(JSContext* ctx, JSValueConst this_val, int magic) {
  return JS_GetPropertyUint32(ctx, this_val, magic);
}

static JSValue
nvgjs_transform_set(JSContext* ctx, JSValueConst this_val, JSValueConst value, int magic) {
  JS_SetPropertyUint32(ctx, this_val, magic, JS_DupValue(ctx, value));
  return JS_UNDEFINED;
}

NVGJS_DECL(Transform, Identity) {
  float dst[6], *trf;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    if(argc >= 1)
      nvgjs_inputoutputarray(ctx, dst, 6, argv[i++]);

    trf = dst;
  }

  nvgTransformIdentity(trf);

  if(i == 0)
    return trf == dst ? nvgjs_transform_new(ctx, dst) : JS_DupValue(ctx, this_obj);

  return nvgjs_transform_copy(ctx, argv[0], dst);
  ;
}

NVGJS_DECL(Transform, Translate) {
  float dst[6], vec[2], *trf;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    if(argc >= 1 && !JS_IsNumber(argv[0]) && !nvgjs_inputoutputarray(ctx, dst, 6, argv[0]))
      i++;

    trf = dst;
  }

  if(!nvgjs_vector_arguments(ctx, vec, argc - i, argv + i))
    return JS_ThrowInternalError(ctx, "need x, y arguments");

  nvgTransformTranslate(trf, vec[0], vec[1]);

  if(i == 0)
    return trf == dst ? nvgjs_transform_new(ctx, dst) : JS_DupValue(ctx, this_obj);

  return nvgjs_transform_copy(ctx, argv[0], dst);
  ;
}

NVGJS_DECL(Transform, Scale) {
  float dst[6], vec[2], *trf;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    if(argc >= 1 && !JS_IsNumber(argv[0]) && !nvgjs_inputoutputarray(ctx, dst, 6, argv[0]))
      i++;

    trf = dst;
  }

  if(!nvgjs_vector_arguments(ctx, vec, argc - i, argv + i))
    return JS_ThrowInternalError(ctx, "need x, y or vector arguments");

  nvgTransformScale(trf, vec[0], vec[1]);

  if(i == 0)
    return trf == dst ? nvgjs_transform_new(ctx, dst) : JS_DupValue(ctx, this_obj);

  return nvgjs_transform_copy(ctx, argv[0], dst);
  ;
}

NVGJS_DECL(Transform, Rotate) {
  float dst[6], *trf;
  double angle;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    if(argc >= 1 && !JS_IsNumber(argv[0]) && !nvgjs_inputoutputarray(ctx, dst, 6, argv[0]))
      i++;

    trf = dst;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need angle argument");

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformRotate(trf, angle);

  if(i == 0)
    return trf == dst ? nvgjs_transform_new(ctx, dst) : JS_DupValue(ctx, this_obj);

  return nvgjs_transform_copy(ctx, argv[0], dst);
  ;
}

NVGJS_DECL(Transform, SkewX) {
  float dst[6], *trf;
  double angle;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    if(argc >= 1 && !JS_IsNumber(argv[0]) && !nvgjs_inputoutputarray(ctx, dst, 6, argv[0]))
      i++;

    trf = dst;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need angle argument");

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformSkewX(trf, angle);

  if(i == 0)
    return trf == dst ? nvgjs_transform_new(ctx, dst) : JS_DupValue(ctx, this_obj);

  return nvgjs_transform_copy(ctx, argv[0], dst);
  ;
}

NVGJS_DECL(Transform, SkewY) {
  float dst[6], *trf;
  double angle;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    if(argc >= 1 && !JS_IsNumber(argv[0]) && !nvgjs_inputoutputarray(ctx, dst, 6, argv[0]))
      i++;

    trf = dst;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need angle argument");

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformSkewY(trf, angle);

  if(i == 0)
    return trf == dst ? nvgjs_transform_new(ctx, dst) : JS_DupValue(ctx, this_obj);

  return nvgjs_transform_copy(ctx, argv[0], dst);
  ;
}

NVGJS_DECL(Transform, Multiply) {
  float dst[6], src[6], *trf;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    nvgjs_inputoutputarray(ctx, dst, 6, argv[0]);
    i++;
    trf = dst;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  for(int n; i < argc && (n = nvgjs_transform_arguments(ctx, src, argc - i, argv + i)); i += n) {
    nvgTransformMultiply(trf, src);
  }

  if(trf != dst)
    return JS_DupValue(ctx, this_obj);

  return nvgjs_transform_copy(ctx, argv[0], dst);
  ;
}

NVGJS_DECL(Transform, Premultiply) {
  float dst[6], src[6], *trf;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    nvgjs_inputoutputarray(ctx, dst, 6, argv[0]);
    i++;
    trf = dst;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  for(int n; i < argc && (n = nvgjs_transform_arguments(ctx, src, argc - i, argv + i)); i += n) {
    nvgTransformPremultiply(trf, src);
  }

  if(trf != dst)
    return JS_DupValue(ctx, this_obj);

  return nvgjs_transform_copy(ctx, argv[0], dst);
  ;
}

NVGJS_DECL(Transform, Inverse) {
  float dst[6], src[6], *trf;
  int i = 0;

  if(!(trf = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 1)
      i++;
    trf = dst;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  nvgjs_inputoutputarray(ctx, src, 6, argv[i]);

  int ret = nvgTransformInverse(trf, src);

  if(!ret)
    return JS_ThrowInternalError(ctx, "nvgTransformInverse failed");

  if(trf != dst)
    return JS_DupValue(ctx, this_obj);

  if(i == 0)
    return nvgjs_transform_new(ctx, dst);

  return nvgjs_transform_copy(ctx, argv[0], dst);
}

static const JSCFunctionListEntry nvgjs_transform_methods[] = {
 JS_CGETSET_MAGIC_DEF("a", nvgjs_transform_get, nvgjs_transform_set, 0),
 JS_CGETSET_MAGIC_DEF("b", nvgjs_transform_get, nvgjs_transform_set, 1),
 JS_CGETSET_MAGIC_DEF("c", nvgjs_transform_get, nvgjs_transform_set, 2),
 JS_CGETSET_MAGIC_DEF("d", nvgjs_transform_get, nvgjs_transform_set, 3),
 JS_CGETSET_MAGIC_DEF("e", nvgjs_transform_get, nvgjs_transform_set, 4),
 JS_CGETSET_MAGIC_DEF("f", nvgjs_transform_get, nvgjs_transform_set, 5),
 JS_PROP_STRING_DEF("[Symbol.toStringTag]", "nvgTransform", JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry nvgjs_transform_functions[] = {
 NVGJS_METHOD(Transform, Identity, 0),
 NVGJS_METHOD(Transform, Translate, 2),
 NVGJS_METHOD(Transform, Scale, 2),
 NVGJS_METHOD(Transform, Rotate, 1),
 NVGJS_METHOD(Transform, SkewX, 1),
 NVGJS_METHOD(Transform, SkewY, 1),
 NVGJS_METHOD(Transform, Multiply, 1),
 NVGJS_METHOD(Transform, Premultiply, 1),
 NVGJS_METHOD(Transform, Inverse, 1),
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

static JSValue
nvgjs_context_wrap(JSContext* ctx, JSValueConst proto, NVGcontext* nvg) {
  JSValue obj = JS_NewObjectProtoClass(ctx, proto, nvgjs_context_class_id);

  if(!JS_IsException(obj))
    JS_SetOpaque(obj, nvg);

  return obj;
}

static void
nvgjs_context_finalizer(JSRuntime* rt, JSValue val) {
  NVGcontext* nvg;

  if((nvg = JS_GetOpaque(val, nvgjs_context_class_id))) {
    // XXX: bug
  }
}

static JSClassDef nvgjs_context_class = {
 .class_name = "NVGcontext",
 .finalizer = nvgjs_context_finalizer,
};

#ifdef NANOVG_GL2
NVGJS_DECL(func, CreateGL2) {
  int32_t flags = 0;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &flags, argv[0]))
    return JS_EXCEPTION;

  return nvgjs_context_wrap(ctxc, context_proto, nvgCreateGL2(flags));
}

NVGJS_DECL(func, DeleteGL2) {
  NVGJS_CONTEXT(argv[0]);

  if(nvg) {
    nvgDeleteGL2(nvg);
    JS_SetOpaque(argv[0], 0);
  }

  return JS_UNDEFINED;
}

NVGJS_DECL(func, CreateImageFromHandleGL2) {
  NVGJS_CONTEXT(argv[0]);

  uint32_t textureId;
  int32_t w, h, imageFlags;

  if(JS_ToUint32(ctx, &textureId, argv[1]))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &w, argv[2]))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &imageFlags, argv[4]))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvglCreateImageFromHandleGL2(nvg, textureId, w, h, imageFlags));
}

NVGJS_DECL(func, ImageHandleGL2) {
  NVGJS_CONTEXT(argv[0]);

  int32_t imageHandle;

  if(JS_ToInt32(ctx, &imageHandle, argv[1]))
    return JS_EXCEPTION;

  return JS_NewUint32(ctx, nvglImageHandleGL2(nvg, imageHandle));
}
#endif

#ifdef NANOVG_GL3
NVGJS_DECL(func, CreateGL3) {
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

  return nvgjs_context_wrap(ctx, context_proto, nvgCreateGL3(flags));
}

NVGJS_DECL(func, DeleteGL3) {
  NVGJS_CONTEXT(argv[0]);

  if(nvg) {
    nvgDeleteGL3(nvg);
    JS_SetOpaque(argv[0], 0);
  }

  return JS_UNDEFINED;
}

NVGJS_DECL(func, CreateImageFromHandleGL3) {
  NVGJS_CONTEXT(argv[0]);

  uint32_t textureId;
  int32_t w, h, imageFlags;

  if(JS_ToUint32(ctx, &textureId, argv[1]))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &w, argv[2]))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &imageFlags, argv[4]))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvglCreateImageFromHandleGL3(nvg, textureId, w, h, imageFlags));
}

NVGJS_DECL(func, ImageHandleGL3) {
  NVGJS_CONTEXT(argv[0]);

  int32_t imageHandle;

  if(JS_ToInt32(ctx, &imageHandle, argv[1]))
    return JS_EXCEPTION;

  return JS_NewUint32(ctx, nvglImageHandleGL3(nvg, imageHandle));
}
#endif

NVGJS_DECL(func, CreateFramebuffer) {
  NVGJS_CONTEXT(argv[0]);

  int32_t w, h, imageFlags;

  if(JS_ToInt32(ctx, &w, argv[1]))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &h, argv[2]))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &imageFlags, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_framebuffer_wrap(ctx, framebuffer_proto, nvgluCreateFramebuffer(nvg, w, h, imageFlags));
}

NVGJS_DECL(func, BindFramebuffer) {
  NVGJS_FRAMEBUFFER(argv[0]);

  nvgluBindFramebuffer(fb);

  return JS_UNDEFINED;
}

NVGJS_DECL(func, DeleteFramebuffer) {
  NVGJS_FRAMEBUFFER(argv[0]);

  nvgluDeleteFramebuffer(fb);
  JS_SetOpaque(argv[0], 0);

  return JS_UNDEFINED;
}

NVGJS_DECL(func, DegToRad) {
  double arg;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &arg, argv[0]))
    return JS_EXCEPTION;

  return JS_NewFloat64(ctx, nvgDegToRad(arg));
}

NVGJS_DECL(func, RadToDeg) {
  double arg;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &arg, argv[0]))
    return JS_EXCEPTION;

  return JS_NewFloat64(ctx, nvgRadToDeg(arg));
}

NVGJS_DECL(func, RGB) {
  int32_t r, g, b;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToInt32(ctx, &r, argv[0]) || JS_ToInt32(ctx, &g, argv[1]) || JS_ToInt32(ctx, &b, argv[2]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGB(r, g, b));
}

NVGJS_DECL(func, RGBf) {
  double r, g, b;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &r, argv[0]) || JS_ToFloat64(ctx, &g, argv[1]) || JS_ToFloat64(ctx, &b, argv[2]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGBf(r, g, b));
}

NVGJS_DECL(func, RGBA) {
  int32_t r, g, b, a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToInt32(ctx, &r, argv[0]) || JS_ToInt32(ctx, &g, argv[1]) || JS_ToInt32(ctx, &b, argv[2]) || JS_ToInt32(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGBA(r, g, b, a));
}

NVGJS_DECL(func, RGBAf) {
  double r, g, b, a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &r, argv[0]) || JS_ToFloat64(ctx, &g, argv[1]) || JS_ToFloat64(ctx, &b, argv[2]) || JS_ToFloat64(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGBAf(r, g, b, a));
}

NVGJS_DECL(func, LerpRGBA) {
  NVGcolor c0, c1;
  double u;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(nvgjs_tocolor(ctx, &c0, argv[0]) || nvgjs_tocolor(ctx, &c1, argv[1]) || JS_ToFloat64(ctx, &u, argv[2]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgLerpRGBA(c0, c1, u));
}

NVGJS_DECL(func, TransRGBA) {
  NVGcolor c;
  int32_t a;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(nvgjs_tocolor(ctx, &c, argv[0]) || JS_ToInt32(ctx, &a, argv[1]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgTransRGBA(c, a));
}

NVGJS_DECL(func, TransRGBAf) {
  NVGcolor c;
  double a;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(nvgjs_tocolor(ctx, &c, argv[0]) || JS_ToFloat64(ctx, &a, argv[1]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgTransRGBAf(c, a));
}

NVGJS_DECL(func, HSL) {
  double h, s, l;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &h, argv[0]) || JS_ToFloat64(ctx, &s, argv[1]) || JS_ToFloat64(ctx, &l, argv[2]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgHSL(h, s, l));
}

NVGJS_DECL(func, HSLA) {
  double h, s, l;
  int32_t a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &h, argv[0]) || JS_ToFloat64(ctx, &s, argv[1]) || JS_ToFloat64(ctx, &l, argv[2]) || JS_ToInt32(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgHSLA(h, s, l, a));
}

NVGJS_DECL(func, TransformPoint) {
  float trf[6], dst[2], src[2];

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  nvgjs_tovector(ctx, dst, argv[0]);
  nvgjs_inputoutputarray(ctx, trf, 6, argv[1]);
  nvgjs_vector_arguments(ctx, src, argc - 2, argv + 2);

  nvgTransformPoint(&dst[0], &dst[1], trf, src[0], src[1]);

  nvgjs_vector_copy(ctx, argv[0], dst);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, CreateFont) {
  NVGJS_CONTEXT(this_obj);

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* file = JS_ToCString(ctx, argv[1]);

  return JS_NewInt32(ctx, nvgCreateFont(nvg, name, file));
}

NVGJS_DECL(Context, CreateFontAtIndex) {
  NVGJS_CONTEXT(this_obj);

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* file = JS_ToCString(ctx, argv[1]);
  int32_t index;

  if(JS_ToInt32(ctx, &index, argv[2]))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateFontAtIndex(nvg, name, file, index));
}

NVGJS_DECL(Context, FindFont) {
  NVGJS_CONTEXT(this_obj);

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  const char* name = JS_ToCString(ctx, argv[0]);

  return JS_NewInt32(ctx, nvgFindFont(nvg, name));
}

NVGJS_DECL(Context, BeginFrame) {
  NVGJS_CONTEXT(this_obj);

  double w, h, ratio;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &w, argv[0]) || JS_ToFloat64(ctx, &h, argv[1]) | JS_ToFloat64(ctx, &ratio, argv[2]))
    return JS_EXCEPTION;

  nvgBeginFrame(nvg, w, h, ratio);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, EndFrame) {
  NVGJS_CONTEXT(this_obj);

  nvgEndFrame(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, CancelFrame) {
  NVGJS_CONTEXT(this_obj);

  nvgCancelFrame(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Save) {
  NVGJS_CONTEXT(this_obj);

  nvgSave(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Restore) {
  NVGJS_CONTEXT(this_obj);

  nvgRestore(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Reset) {
  NVGJS_CONTEXT(this_obj);

  nvgReset(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, ShapeAntiAlias) {
  NVGJS_CONTEXT(this_obj);

  nvgShapeAntiAlias(nvg, JS_ToBool(ctx, argv[0]));
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Rect) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgRect(nvg, x, y, w, h);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Circle) {
  NVGJS_CONTEXT(this_obj);

  double cx, cy, r;

  if(argc < 3)
    return JS_ThrowInternalError(ctx, "need 3 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &r, argv[2]))
    return JS_EXCEPTION;

  nvgCircle(nvg, cx, cy, r);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Ellipse) {
  NVGJS_CONTEXT(this_obj);

  double cx, cy, rx, ry;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &rx, argv[2]) || JS_ToFloat64(ctx, &ry, argv[3]))
    return JS_EXCEPTION;

  nvgEllipse(nvg, cx, cy, rx, ry);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, PathWinding) {
  NVGJS_CONTEXT(this_obj);

  int dir;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &dir, argv[0]))
    return JS_EXCEPTION;

  nvgPathWinding(nvg, dir);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, MoveTo) {
  NVGJS_CONTEXT(this_obj);

  double x, y;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgMoveTo(nvg, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, LineTo) {
  NVGJS_CONTEXT(this_obj);

  double x, y;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgLineTo(nvg, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, BezierTo) {
  NVGJS_CONTEXT(this_obj);

  double c1x, c1y, c2x, c2y, x, y;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &c1x, argv[0]) || JS_ToFloat64(ctx, &c1y, argv[1]) || JS_ToFloat64(ctx, &c2x, argv[2]) || JS_ToFloat64(ctx, &c2y, argv[3]) || JS_ToFloat64(ctx, &x, argv[4]) || JS_ToFloat64(ctx, &y, argv[5]))
    return JS_EXCEPTION;

  nvgBezierTo(nvg, c1x, c1y, c2x, c2y, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, QuadTo) {
  NVGJS_CONTEXT(this_obj);

  double cx, cy, x, y;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &x, argv[2]) || JS_ToFloat64(ctx, &y, argv[3]))
    return JS_EXCEPTION;

  nvgQuadTo(nvg, cx, cy, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, ArcTo) {
  NVGJS_CONTEXT(this_obj);

  double x1, y1, x2, y2, radius;

  if(argc < 5)
    return JS_ThrowInternalError(ctx, "need 5 arguments");

  if(JS_ToFloat64(ctx, &x1, argv[0]) || JS_ToFloat64(ctx, &y1, argv[1]) || JS_ToFloat64(ctx, &x2, argv[2]) || JS_ToFloat64(ctx, &y2, argv[3]) || JS_ToFloat64(ctx, &radius, argv[4]))
    return JS_EXCEPTION;

  nvgArcTo(nvg, x1, y1, x2, y2, radius);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Arc) {
  NVGJS_CONTEXT(this_obj);

  double cx, cy, r, a0, a1;
  int32_t dir;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &r, argv[2]) || JS_ToFloat64(ctx, &a0, argv[3]) || JS_ToFloat64(ctx, &a1, argv[4]) || JS_ToInt32(ctx, &dir, argv[5]))
    return JS_EXCEPTION;

  nvgArc(nvg, cx, cy, r, a0, a1, dir);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, ClosePath) {
  NVGJS_CONTEXT(this_obj);

  nvgClosePath(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, FontSize) {
  NVGJS_CONTEXT(this_obj);

  double size;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &size, argv[0]))
    return JS_EXCEPTION;

  nvgFontSize(nvg, size);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, FontBlur) {
  NVGJS_CONTEXT(this_obj);

  double blur;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &blur, argv[0]))
    return JS_EXCEPTION;

  nvgFontBlur(nvg, blur);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, TextLetterSpacing) {
  NVGJS_CONTEXT(this_obj);

  double spacing;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &spacing, argv[0]))
    return JS_EXCEPTION;

  nvgTextLetterSpacing(nvg, spacing);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, TextLineHeight) {
  NVGJS_CONTEXT(this_obj);

  double height;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &height, argv[0]))
    return JS_EXCEPTION;

  nvgTextLineHeight(nvg, height);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, BeginPath) {
  NVGJS_CONTEXT(this_obj);

  nvgBeginPath(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, RoundedRect) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h, r;

  if(argc < 5)
    return JS_ThrowInternalError(ctx, "need 5 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &r, argv[4]))
    return JS_EXCEPTION;

  nvgRoundedRect(nvg, x, y, w, h, r);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, RoundedRectVarying) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h, rtl, rtr, rbr, rbl;

  if(argc < 8)
    return JS_ThrowInternalError(ctx, "need 8 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &rtl, argv[4]) || JS_ToFloat64(ctx, &rtr, argv[5]) || JS_ToFloat64(ctx, &rbr, argv[6]) || JS_ToFloat64(ctx, &rbl, argv[7]))
    return JS_EXCEPTION;

  nvgRoundedRectVarying(nvg, x, y, w, h, rtl, rtr, rbr, rbl);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Scissor) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgScissor(nvg, x, y, w, h);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, IntersectScissor) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgIntersectScissor(nvg, x, y, w, h);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, ResetScissor) {
  NVGJS_CONTEXT(this_obj);

  nvgResetScissor(nvg);
}

NVGJS_DECL(Context, FillPaint) {
  NVGJS_CONTEXT(this_obj);

  NVGpaint* paint;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(!(paint = JS_GetOpaque2(ctx, argv[0], nvgjs_paint_class_id)))
    return JS_EXCEPTION;

  nvgFillPaint(nvg, *paint);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Fill) {
  NVGJS_CONTEXT(this_obj);

  nvgFill(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, MiterLimit) {
  NVGJS_CONTEXT(this_obj);

  double miterLimit;

  if(JS_ToFloat64(ctx, &miterLimit, argv[0]))
    return JS_EXCEPTION;

  nvgMiterLimit(nvg, miterLimit);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, LineCap) {
  NVGJS_CONTEXT(this_obj);

  int32_t lineCap;

  if(JS_ToInt32(ctx, &lineCap, argv[0]))
    return JS_EXCEPTION;

  nvgLineCap(nvg, lineCap);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, LineJoin) {
  NVGJS_CONTEXT(this_obj);

  int32_t lineJoin;

  if(JS_ToInt32(ctx, &lineJoin, argv[0]))
    return JS_EXCEPTION;

  nvgLineCap(nvg, lineJoin);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, GlobalAlpha) {
  NVGJS_CONTEXT(this_obj);

  double alpha;

  if(JS_ToFloat64(ctx, &alpha, argv[0]))
    return JS_EXCEPTION;

  nvgGlobalAlpha(nvg, alpha);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, StrokeColor) {
  NVGJS_CONTEXT(this_obj);

  NVGcolor color;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(nvgjs_tocolor(ctx, &color, argv[0]))
    return JS_EXCEPTION;

  nvgStrokeColor(nvg, color);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Stroke) {
  NVGJS_CONTEXT(this_obj);

  nvgStroke(nvg);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, StrokePaint) {
  NVGJS_CONTEXT(this_obj);

  NVGpaint* paint;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(!(paint = JS_GetOpaque(argv[0], nvgjs_paint_class_id)))
    return JS_EXCEPTION;

  nvgStrokePaint(nvg, *paint);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, FillColor) {
  NVGJS_CONTEXT(this_obj);

  NVGcolor color;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(nvgjs_tocolor(ctx, &color, argv[0]))
    return JS_EXCEPTION;

  nvgFillColor(nvg, color);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, LinearGradient) {
  NVGJS_CONTEXT(this_obj);

  double sx, sy, ex, ey;
  NVGcolor icol, ocol;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &sx, argv[0]) || JS_ToFloat64(ctx, &sy, argv[1]) || JS_ToFloat64(ctx, &ex, argv[2]) || JS_ToFloat64(ctx, &ey, argv[3]) || nvgjs_tocolor(ctx, &icol, argv[4]) || nvgjs_tocolor(ctx, &ocol, argv[5]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgLinearGradient(nvg, sx, sy, ex, ey, icol, ocol));
}

NVGJS_DECL(Context, BoxGradient) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h, r, f;
  NVGcolor icol, ocol;

  if(argc < 8)
    return JS_ThrowInternalError(ctx, "need 8 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) || JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &r, argv[4]) || JS_ToFloat64(ctx, &f, argv[5]) || nvgjs_tocolor(ctx, &icol, argv[6]) || nvgjs_tocolor(ctx, &ocol, argv[7]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgBoxGradient(nvg, x, y, w, h, r, f, icol, ocol));
}

NVGJS_DECL(Context, RadialGradient) {
  NVGJS_CONTEXT(this_obj);

  double cx, cy, inr, outr;
  NVGcolor icol, ocol;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &inr, argv[2]) || JS_ToFloat64(ctx, &outr, argv[3]) || nvgjs_tocolor(ctx, &icol, argv[4]) || nvgjs_tocolor(ctx, &ocol, argv[5]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgRadialGradient(nvg, cx, cy, inr, outr, icol, ocol));
}

NVGJS_DECL(Context, TextAlign) {
  NVGJS_CONTEXT(this_obj);

  int align;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &align, argv[0]))
    return JS_EXCEPTION;

  nvgTextAlign(nvg, align);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, FontFace) {
  NVGJS_CONTEXT(this_obj);

  const char* str;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(!(str = JS_ToCString(ctx, argv[0])))
    return JS_EXCEPTION;

  nvgFontFace(nvg, str);
  JS_FreeCString(ctx, str);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Text) {
  NVGJS_CONTEXT(this_obj);

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

  float ret = nvgText(nvg, x, y, str, end);

  JS_FreeCString(ctx, str);

  return JS_NewFloat64(ctx, ret);
}

NVGJS_DECL(Context, TextBox) {
  NVGJS_CONTEXT(this_obj);

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

  nvgTextBox(nvg, x, y, breakRowWidth, str, end);

  JS_FreeCString(ctx, str);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, TextBounds) {
  NVGJS_CONTEXT(this_obj);

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

  float ret = nvgTextBounds(nvg, x, y, str, end, bounds);

  JS_FreeCString(ctx, str);

  nvgjs_copyobject(ctx, argv[4], (const char* const[]){"xmin", "ymin", "xmax", "ymax"}, bounds, countof(bounds));

  return JS_NewFloat64(ctx, ret);
}

NVGJS_DECL(Context, TextBoxBounds) {
  NVGJS_CONTEXT(this_obj);

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

  nvgTextBoxBounds(nvg, x, y, breakRowWidth, str, end, bounds);

  JS_FreeCString(ctx, str);

  nvgjs_copyobject(ctx, argv[5], (const char* const[]){"xmin", "ymin", "xmax", "ymax"}, bounds, countof(bounds));

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, TextBounds2) {
  NVGJS_CONTEXT(this_obj);

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
    float tw = nvgTextBounds(nvg, x, y, str, NULL, bounds);

    JSValue e = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, e, "width", JS_NewFloat64(ctx, tw), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, e, "height", JS_NewFloat64(ctx, bounds[3] - bounds[1]), JS_PROP_C_W_E);
    return e;
  }
}

NVGJS_DECL(Context, StrokeWidth) {
  NVGJS_CONTEXT(this_obj);

  double width;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &width, argv[0]))
    return JS_EXCEPTION;

  nvgStrokeWidth(nvg, width);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, CreateImage) {
  NVGJS_CONTEXT(this_obj);

  const char* file;
  int32_t flags = 0;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(!(file = JS_ToCString(ctx, argv[0])))
    return JS_EXCEPTION;

  if(JS_ToInt32(ctx, &flags, argv[1]))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateImage(nvg, file, flags));
}

NVGJS_DECL(Context, CreateImageMem) {
  NVGJS_CONTEXT(this_obj);

  int32_t flags;
  uint8_t* ptr;
  size_t len;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToInt32(ctx, &flags, argv[0]))
    return JS_EXCEPTION;

  if(!(ptr = JS_GetArrayBuffer(ctx, &len, argv[1])))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateImageMem(nvg, flags, (void*)ptr, len));
}

NVGJS_DECL(Context, CreateImageRGBA) {
  NVGJS_CONTEXT(this_obj);

  int32_t width, height, flags;
  uint8_t* ptr;
  size_t len;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToInt32(ctx, &width, argv[0]) || JS_ToInt32(ctx, &height, argv[1]) || JS_ToInt32(ctx, &flags, argv[2]))
    return JS_EXCEPTION;

  if(!(ptr = JS_GetArrayBuffer(ctx, &len, argv[3])))
    return JS_EXCEPTION;

  return JS_NewInt32(ctx, nvgCreateImageRGBA(nvg, width, height, flags, (void*)ptr));
}

NVGJS_DECL(Context, UpdateImage) {
  NVGJS_CONTEXT(this_obj);

  int32_t image;
  size_t len;
  uint8_t* ptr;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToInt32(ctx, &image, argv[0]))
    return JS_EXCEPTION;

  if(!(ptr = JS_GetArrayBuffer(ctx, &len, argv[1])))
    return JS_EXCEPTION;

  nvgUpdateImage(nvg, image, (void*)ptr);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, ImageSize) {
  NVGJS_CONTEXT(this_obj);

  int32_t id = 0;
  int width, height;
  JSValue ret;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &id, argv[0]))
    return JS_EXCEPTION;

  nvgImageSize(nvg, id, &width, &height);

  ret = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, ret, 0, JS_NewInt32(ctx, width));
  JS_SetPropertyUint32(ctx, ret, 1, JS_NewInt32(ctx, height));
  return ret;
}

NVGJS_DECL(Context, DeleteImage) {
  NVGJS_CONTEXT(this_obj);

  int32_t id = 0;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &id, argv[0]))
    return JS_EXCEPTION;

  nvgDeleteImage(nvg, id);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, ResetTransform) {
  NVGJS_CONTEXT(this_obj);

  nvgResetTransform(nvg);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Transform) {
  NVGJS_CONTEXT(this_obj);

  float t[6];
  int n;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1/6 arguments");

  while((n = nvgjs_transform_arguments(ctx, t, argc, argv))) {
    nvgTransform(nvg, t[0], t[1], t[2], t[3], t[4], t[5]);

    argc -= n;
    argv += n;
  }

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Translate) {
  NVGJS_CONTEXT(this_obj);

  double x, y;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgTranslate(nvg, x, y);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Rotate) {
  NVGJS_CONTEXT(this_obj);

  double angle;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgRotate(nvg, angle);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, SkewX) {
  NVGJS_CONTEXT(this_obj);

  double angle;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgSkewX(nvg, angle);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, SkewY) {
  NVGJS_CONTEXT(this_obj);

  double angle;

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToFloat64(ctx, &angle, argv[0]))
    return JS_EXCEPTION;

  nvgSkewY(nvg, angle);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Scale) {
  NVGJS_CONTEXT(this_obj);

  double x, y;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]))
    return JS_EXCEPTION;

  nvgScale(nvg, x, y);

  return JS_UNDEFINED;
}

NVGJS_DECL(Context, CurrentTransform) {
  NVGJS_CONTEXT(this_obj);

  float t[6];

  nvgCurrentTransform(nvg, t);

  if(argc == 0)
    return nvgjs_transform_new(ctx, t);

  return nvgjs_transform_copy(ctx, argv[0], t);
  ;
}

NVGJS_DECL(Context, ImagePattern) {
  NVGJS_CONTEXT(this_obj);

  double ox, oy, ex, ey;
  double angle, alpha;
  int32_t image;
  NVGpaint paint, *p;

  if(argc < 7)
    return JS_ThrowInternalError(ctx, "need 7 arguments");

  if(JS_ToFloat64(ctx, &ox, argv[0]) || JS_ToFloat64(ctx, &oy, argv[1]) || JS_ToFloat64(ctx, &ex, argv[2]) || JS_ToFloat64(ctx, &ey, argv[3]) || JS_ToFloat64(ctx, &angle, argv[4]) || JS_ToInt32(ctx, &image, argv[5]) || JS_ToFloat64(ctx, &alpha, argv[6]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgImagePattern(nvg, ox, oy, ex, ey, angle, image, alpha));
}

/*NVGJS_DECL(Context, SetNextFillHoverable)
{
    nvgSetNextFillHoverable(nvg);
    return JS_UNDEFINED;
}

NVGJS_DECL(Context, IsFillHovered)
{
    int ret = nvgIsFillHovered(nvg);
    return JS_NewBool(ctx, ret);
}

NVGJS_DECL(Context, IsNextFillClicked)
{
    int ret = nvgIsNextFillClicked(nvg);
    return JS_NewBool(ctx, ret);
}
*/

static const JSCFunctionListEntry nvgjs_funcs[] = {
#ifdef NANOVG_GL2
 NVGJS_FUNC(CreateGL2, 1),
 NVGJS_FUNC(DeleteGL2, 1),
 NVGJS_FUNC(CreateImageFromHandleGL2, 5),
 NVGJS_FUNC(ImageHandleGL2, 2),
#endif
#ifdef NANOVG_GL3
 NVGJS_FUNC(CreateGL3, 1),
 NVGJS_FUNC(DeleteGL3, 1),
 NVGJS_FUNC(CreateImageFromHandleGL3, 5),
 NVGJS_FUNC(ImageHandleGL3, 2),
#endif

 NVGJS_FLAG(STENCIL_STROKES),
 NVGJS_FLAG(ANTIALIAS),
 NVGJS_FLAG(DEBUG),

 NVGJS_FLAG(IMAGE_NODELETE),

 NVGJS_FUNC(CreateFramebuffer, 4),
 NVGJS_FUNC(BindFramebuffer, 1),
 NVGJS_FUNC(DeleteFramebuffer, 1),

 NVGJS_FUNC(RadToDeg, 1),
 NVGJS_FUNC(DegToRad, 1),
 NVGJS_FUNC(RGB, 3),
 NVGJS_FUNC(RGBf, 3),
 NVGJS_FUNC(RGBA, 4),
 NVGJS_FUNC(RGBAf, 4),
 NVGJS_FUNC(LerpRGBA, 3),
 NVGJS_FUNC(TransRGBA, 2),
 NVGJS_FUNC(TransRGBAf, 2),
 NVGJS_FUNC(HSL, 3),
 NVGJS_FUNC(HSLA, 4),

 NVGJS_FUNC(TransformPoint, 2),

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

static const JSCFunctionListEntry nvgjs_context_methods[] = {
 NVGJS_METHOD(Context, CreateFont, 2),
 NVGJS_METHOD(Context, CreateFontAtIndex, 3),
 NVGJS_METHOD(Context, FindFont, 1),
 NVGJS_METHOD(Context, BeginFrame, 3),
 NVGJS_METHOD(Context, CancelFrame, 0),
 NVGJS_METHOD(Context, EndFrame, 0),
 NVGJS_METHOD(Context, Save, 0),
 NVGJS_METHOD(Context, Restore, 0),
 NVGJS_METHOD(Context, Reset, 0),
 NVGJS_METHOD(Context, ShapeAntiAlias, 1),
 NVGJS_METHOD(Context, ClosePath, 0),
 NVGJS_METHOD(Context, Scissor, 4),
 NVGJS_METHOD(Context, IntersectScissor, 4),
 NVGJS_METHOD(Context, ResetScissor, 0),

 NVGJS_METHOD(Context, MiterLimit, 1),
 NVGJS_METHOD(Context, LineCap, 1),
 NVGJS_METHOD(Context, LineJoin, 1),
 NVGJS_METHOD(Context, GlobalAlpha, 1),
 NVGJS_METHOD(Context, StrokeColor, 1),
 NVGJS_METHOD(Context, StrokeWidth, 1),
 NVGJS_METHOD(Context, StrokePaint, 1),
 NVGJS_METHOD(Context, FillColor, 1),
 NVGJS_METHOD(Context, FillPaint, 1),
 NVGJS_METHOD(Context, LinearGradient, 6),
 NVGJS_METHOD(Context, BoxGradient, 8),
 NVGJS_METHOD(Context, RadialGradient, 6),
 NVGJS_METHOD(Context, FontSize, 1),
 NVGJS_METHOD(Context, FontBlur, 1),
 NVGJS_METHOD(Context, TextLetterSpacing, 1),
 NVGJS_METHOD(Context, TextLineHeight, 1),
 NVGJS_METHOD(Context, TextAlign, 1),
 NVGJS_METHOD(Context, FontFace, 1),
 NVGJS_METHOD(Context, Text, 3),
 NVGJS_METHOD(Context, TextBox, 4),
 NVGJS_METHOD(Context, TextBounds, 5),
 NVGJS_METHOD(Context, TextBoxBounds, 6),
 NVGJS_METHOD(Context, TextBounds2, 3),

 NVGJS_METHOD(Context, CreateImage, 2),
 NVGJS_METHOD(Context, CreateImageMem, 2),
 NVGJS_METHOD(Context, CreateImageRGBA, 4),
 NVGJS_METHOD(Context, UpdateImage, 2),
 NVGJS_METHOD(Context, ImageSize, 1),
 NVGJS_METHOD(Context, DeleteImage, 1),
 NVGJS_METHOD(Context, ResetTransform, 0),
 NVGJS_METHOD(Context, Transform, 6),
 NVGJS_METHOD(Context, Translate, 2),
 NVGJS_METHOD(Context, Rotate, 1),
 NVGJS_METHOD(Context, SkewX, 1),
 NVGJS_METHOD(Context, SkewY, 1),
 NVGJS_METHOD(Context, Scale, 2),
 NVGJS_METHOD(Context, CurrentTransform, 1),
 NVGJS_METHOD(Context, ImagePattern, 7),
 NVGJS_METHOD(Context, BeginPath, 0),
 NVGJS_METHOD(Context, MoveTo, 2),
 NVGJS_METHOD(Context, LineTo, 2),
 NVGJS_METHOD(Context, BezierTo, 6),
 NVGJS_METHOD(Context, QuadTo, 4),
 NVGJS_METHOD(Context, ArcTo, 5),
 NVGJS_METHOD(Context, Arc, 6),
 NVGJS_METHOD(Context, Rect, 4),
 NVGJS_METHOD(Context, Circle, 3),
 NVGJS_METHOD(Context, Ellipse, 4),
 NVGJS_METHOD(Context, RoundedRect, 5),
 NVGJS_METHOD(Context, RoundedRectVarying, 8),
 NVGJS_METHOD(Context, PathWinding, 1),
 NVGJS_METHOD(Context, Stroke, 0),
 NVGJS_METHOD(Context, Fill, 0),
 /*NVGJS_FUNC(SetNextFillHoverable, 0),
 NVGJS_FUNC(IsFillHovered, 0),
 NVGJS_FUNC(IsNextFillClicked, 0),*/
 JS_PROP_STRING_DEF("[Symbol.toStringTag]", "NVGcontext", JS_PROP_CONFIGURABLE),
};

static JSValue
nvgjs_framebuffer_wrap(JSContext* ctx, JSValueConst proto, NVGLUframebuffer* fb) {
  JSValue obj = JS_NewObjectProtoClass(ctx, proto, nvgjs_framebuffer_class_id);

  if(!JS_IsException(obj))
    JS_SetOpaque(obj, fb);

  return obj;
}

static const JSCFunctionListEntry nvgjs_framebuffer_methods[] = {
 JS_PROP_STRING_DEF("[Symbol.toStringTag]", "NVGLUframebuffer", JS_PROP_CONFIGURABLE),
};

static void
nvgjs_framebuffer_finalizer(JSRuntime* rt, JSValue val) {
  NVGLUframebuffer* fb;

  if((fb = JS_GetOpaque(val, nvgjs_framebuffer_class_id))) {
    nvgluDeleteFramebuffer(fb);
  }
}

static JSClassDef nvgjs_framebuffer_class = {
 .class_name = "NVGLUframebuffer",
 .finalizer = nvgjs_framebuffer_finalizer,
};

static int
nvgjs_init(JSContext* ctx, JSModuleDef* m) {
  JSValue paint_proto, paint_class;

  JSValue global = JS_GetGlobalObject(ctx);
  js_float32array_ctor = JS_GetPropertyStr(ctx, global, "Float32Array");
  js_float32array_proto = JS_GetPropertyStr(ctx, js_float32array_ctor, "prototype");
  JS_FreeValue(ctx, global);

  JS_NewClassID(&nvgjs_context_class_id);
  JS_NewClass(JS_GetRuntime(ctx), nvgjs_context_class_id, &nvgjs_context_class);

  context_proto = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetPropertyFunctionList(ctx, context_proto, nvgjs_context_methods, countof(nvgjs_context_methods));
  context_ctor = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetConstructor(ctx, context_ctor, context_proto);
  JS_SetModuleExport(ctx, m, "Context", context_ctor);

  color_proto = JS_NewObjectProto(ctx, js_float32array_proto);
  JS_SetPropertyFunctionList(ctx, color_proto, nvgjs_color_methods, countof(nvgjs_color_methods));
  color_ctor = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetConstructor(ctx, color_ctor, color_proto);
  JS_SetModuleExport(ctx, m, "Color", color_ctor);

  transform_proto = JS_NewObjectProto(ctx, js_float32array_proto);
  JS_SetPropertyFunctionList(ctx, transform_proto, nvgjs_transform_methods, countof(nvgjs_transform_methods));
  transform_ctor = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetPropertyFunctionList(ctx, transform_ctor, nvgjs_transform_functions, countof(nvgjs_transform_functions));
  // JS_SetConstructor(ctx, transform_ctor, transform_proto);
  JS_SetModuleExport(ctx, m, "Transform", transform_ctor);

  JS_NewClassID(&nvgjs_paint_class_id);
  JS_NewClass(JS_GetRuntime(ctx), nvgjs_paint_class_id, &nvgjs_paint_class);

  paint_proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, paint_proto, nvgjs_paint_methods, countof(nvgjs_paint_methods));
  JS_SetClassProto(ctx, nvgjs_paint_class_id, paint_proto);
  paint_class = JS_NewObjectProto(ctx, JS_NULL);
  // JS_SetConstructor(ctx, paint_class, paint_proto);
  JS_SetModuleExport(ctx, m, "Paint", paint_class);

  JS_NewClassID(&nvgjs_framebuffer_class_id);
  JS_NewClass(JS_GetRuntime(ctx), nvgjs_framebuffer_class_id, &nvgjs_framebuffer_class);

  framebuffer_proto = JS_NewObjectProto(ctx, JS_NULL);
  JS_SetPropertyFunctionList(ctx, framebuffer_proto, nvgjs_framebuffer_methods, countof(nvgjs_framebuffer_methods));

  // JS_SetModuleExport(ctx, m, "Framebuffer", framebuffer_ctor);

  JS_SetModuleExportList(ctx, m, nvgjs_funcs, countof(nvgjs_funcs));
  return 0;
}

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

VISIBLE JSModuleDef*
nvgjs_init_module(JSContext* ctx, const char* module_name) {
  JSModuleDef* m;

  if(!(m = JS_NewCModule(ctx, module_name, nvgjs_init)))
    return NULL;

  JS_AddModuleExport(ctx, m, "Context");
  JS_AddModuleExport(ctx, m, "Color");
  JS_AddModuleExport(ctx, m, "Transform");
  JS_AddModuleExport(ctx, m, "Paint");
  // JS_AddModuleExport(ctx, m, "Framebuffer");
  JS_AddModuleExportList(ctx, m, nvgjs_funcs, countof(nvgjs_funcs));
  return m;
}
