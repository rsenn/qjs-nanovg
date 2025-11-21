#ifdef NANOVG_GLEW
#include <GL/glew.h>
#endif

#include "nanovg.h"
#include "nanovg_gl.h"
#include "nanovg_gl_utils.h"

#include "nvgjs-module.h"
#include "nvgjs-utils.h"

#include <assert.h>

JSClassID nvgjs_context_class_id, nvgjs_paint_class_id, nvgjs_framebuffer_class_id;

static JSValue js_float32array_ctor, js_float32array_proto;
static JSValue color_ctor, color_proto;
static JSValue transform_ctor, transform_proto;
static JSValue context_ctor, context_proto;
static JSValue framebuffer_ctor, framebuffer_proto;

static JSValue nvgjs_framebuffer_wrap(JSContext*, JSValueConst, NVGLUframebuffer*);

static const char* const nvgjs_color_keys[] = {"r", "g", "b", "a"};

static void
nvgjs_copy(JSContext* ctx, JSValueConst value, const char* const prop_map[], const float vec[], int vlen) {
  if(JS_IsArray(ctx, value) == TRUE)
    nvgjs_copyarray(ctx, value, vec, vlen);

  nvgjs_copyobject(ctx, value, prop_map, vec, vlen);
}

static float*
nvgjs_transform_output(JSContext* ctx, JSValueConst value) {
  if(nvgjs_same_object(value, transform_ctor))
    return 0;

  return nvgjs_output(ctx, 6, value);
}

static const char* const nvgjs_transform_keys[] = {"a", "b", "c", "d", "e", "f"};

static JSValue
nvgjs_transform_copy(JSContext* ctx, JSValueConst value, const float transform[6]) {
  assert(JS_IsObject(value));
  assert(!nvgjs_same_object(value, transform_ctor));

  nvgjs_copy(ctx, value, nvgjs_transform_keys, transform, 6);
  return JS_UNDEFINED;
}

static const char* const nvgjs_vector_keys[] = {"x", "y"};

static int
nvgjs_tocolor(JSContext* ctx, NVGcolor* color, JSValueConst value) {
  if(nvgjs_inputarray(ctx, color->rgba, 4, value)) {
    color->a = 1;

    return nvgjs_inputarray(ctx, color->rgba, 3, value);
  }
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
nvgjs_point_new(JSContext* ctx, float point[2]) {
  JSValue ret = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, ret, 0, JS_NewFloat64(ctx, point[0]));
  JS_SetPropertyUint32(ctx, ret, 1, JS_NewFloat64(ctx, point[1]));
  return ret;
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
  float tmp[6], *mat;
  int i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    mat = tmp;

    if(argc > 0) {
      if(!(mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[i++])))
        return JS_EXCEPTION;
    }
  }

  nvgTransformIdentity(mat);

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return i ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

NVGJS_DECL(Transform, Translate) {
  float tmp[6], vec[2], *mat;
  int i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 1 && JS_IsObject(argv[0]) && (mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[0])))
      i++;
    else
      mat = tmp;
  }

  if(!nvgjs_arguments(ctx, vec, 2, argc - i, argv + i))
    return JS_ThrowInternalError(ctx, "need x, y arguments");

  nvgTransformTranslate(mat, vec[0], vec[1]);

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return i ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

NVGJS_DECL(Transform, Scale) {
  float tmp[6], vec[2], *mat;
  int i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 1 && JS_IsObject(argv[0]) && (mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[0])))
      i++;
    else
      mat = tmp;
  }

  if(!nvgjs_arguments(ctx, vec, 2, argc - i, argv + i))
    return JS_ThrowInternalError(ctx, "need x, y or vector arguments");

  nvgTransformScale(mat, vec[0], vec[1]);

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return i ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

NVGJS_DECL(Transform, Rotate) {
  float tmp[6], *mat;
  double angle;
  int i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 1 && JS_IsObject(argv[0]) && (mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[0])))
      i++;
    else
      mat = tmp;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need angle argument");

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformRotate(mat, angle);

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return i ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

NVGJS_DECL(Transform, SkewX) {
  float tmp[6], *mat;
  double angle;
  int i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 1 && JS_IsObject(argv[0]) && (mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[0])))
      i++;
    else
      mat = tmp;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need angle argument");

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformSkewX(mat, angle);

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return i ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

NVGJS_DECL(Transform, SkewY) {
  float tmp[6], *mat;
  double angle;
  int i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 1 && JS_IsObject(argv[0]) && (mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[0])))
      i++;
    else
      mat = tmp;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need angle argument");

  if(JS_ToFloat64(ctx, &angle, argv[i]))
    return JS_EXCEPTION;

  nvgTransformSkewY(mat, angle);

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return i ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

NVGJS_DECL(Transform, Multiply) {
  float tmp[6], src[6], *mat;
  int n, i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 1 && (mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[0])))
      i++;
    else
      mat = tmp;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  while(i < argc) {
    if(!(n = nvgjs_arguments(ctx, src, 6, argc - i, argv + i)))
      break;

    nvgTransformMultiply(mat, src);

    i += n;
  }

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return mat == NULL ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

NVGJS_DECL(Transform, Premultiply) {
  float tmp[6], src[6], *mat;
  int i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 1 && (mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[0])))
      i++;
    else
      mat = tmp;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  for(int n; i < argc; i += n) {
    if(!(n = nvgjs_arguments(ctx, src, 6, argc - i, argv + i)))
      break;

    nvgTransformPremultiply(mat, src);
  }

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return mat == NULL ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

NVGJS_DECL(Transform, Inverse) {
  float tmp[6], src[6], *mat;
  int ret, i = 0;

  if(!(mat = nvgjs_transform_output(ctx, this_obj))) {
    if(argc > 0 && (mat = nvgjs_inputoutputarray(ctx, tmp, 6, argv[0])))
      i++;
    else
      mat = tmp;
  }

  if(argc < 1 + i)
    return JS_ThrowInternalError(ctx, "need %d arguments", 1 + i);

  nvgjs_inputarray(ctx, src, 6, argv[i]);

  if(!(ret = nvgTransformInverse(mat, src)))
    return JS_ThrowInternalError(ctx, "nvgTransformInverse failed");

  if(mat == tmp)
    return i ? nvgjs_transform_copy(ctx, argv[0], tmp) : nvgjs_transform_new(ctx, tmp);

  return mat == NULL ? JS_UNDEFINED : JS_DupValue(ctx, this_obj);
}

enum {
  TRANSFORM_TRANSLATE,
  TRANSFORM_ROTATE,
  TRANSFORM_SCALE,
  TRANSFORM_SKEWX,
  TRANSFORM_SKEWY,
  TRANSFORM_INVERSE,
  TRANSFORM_MULTIPLY,
  TRANSFORM_PREMULTIPLY,
  TRANSFORM_POINT,
};

static JSValue
nvgjs_transform_function(JSContext* ctx, JSValueConst this_obj, int argc, JSValueConst argv[], int magic) {

  float *mat = nvgjs_transform_output(ctx, this_obj), tmp[6];

  switch(magic) {
    case TRANSFORM_TRANSLATE: {
      float x, y;
      nvgjs_tofloat32(ctx, &x, argv[0]);
      nvgjs_tofloat32(ctx, &y, argv[1]);
      nvgTransformTranslate(tmp, x, y);
      break;
    }
    case TRANSFORM_SCALE: {
      float x, y;
      if(argc > 0)
        nvgjs_tofloat32(ctx, &x, argv[0]);
      else
        x = 1;
      if(argc > 1)
        nvgjs_tofloat32(ctx, &y, argv[1]);
      else
        y = x;

      nvgTransformScale(tmp, x, y);
      break;
    }
    case TRANSFORM_ROTATE: {
      float a;
      nvgjs_tofloat32(ctx, &a, argv[0]);
      nvgTransformRotate(tmp, a);
      break;
    }
    case TRANSFORM_SKEWX: {
      float a;
      nvgjs_tofloat32(ctx, &a, argv[0]);
      nvgTransformSkewX(tmp, a);
      break;
    }
    case TRANSFORM_SKEWY: {
      float a;
      nvgjs_tofloat32(ctx, &a, argv[0]);
      nvgTransformSkewY(tmp, a);
      break;
    }

    case TRANSFORM_MULTIPLY: {
      if(!nvgjs_arguments(ctx, tmp, 6, argc, argv))
        nvgTransformIdentity(tmp);
      break;

      nvgTransformMultiply(mat, tmp);
      nvgTransformIdentity(tmp);
    }

    case TRANSFORM_PREMULTIPLY: {
      if(!nvgjs_arguments(ctx, tmp, 6, argc, argv))
        nvgTransformIdentity(tmp);

      nvgTransformPremultiply(mat, tmp);
      nvgTransformIdentity(tmp);
      break;
    }

    case TRANSFORM_INVERSE: {
      memcpy(tmp, mat, sizeof(float) * 6);
      nvgTransformInverse(mat, tmp);
      nvgTransformIdentity(tmp);
      break;
    }

    case TRANSFORM_POINT: {
      float in[2], out[2];
      nvgjs_arguments(ctx, in, 2, argc, argv);

      nvgTransformPoint(&out[0], &out[1], mat, in[0], in[1]);

      return nvgjs_point_new(ctx, out);
    }
  }

  nvgTransformPremultiply(mat, tmp);
  return JS_DupValue(ctx, this_obj);
}

static const JSCFunctionListEntry nvgjs_transform_methods[] = {
    JS_CGETSET_MAGIC_DEF("a", nvgjs_transform_get, nvgjs_transform_set, 0),
    JS_CGETSET_MAGIC_DEF("b", nvgjs_transform_get, nvgjs_transform_set, 1),
    JS_CGETSET_MAGIC_DEF("c", nvgjs_transform_get, nvgjs_transform_set, 2),
    JS_CGETSET_MAGIC_DEF("d", nvgjs_transform_get, nvgjs_transform_set, 3),
    JS_CGETSET_MAGIC_DEF("e", nvgjs_transform_get, nvgjs_transform_set, 4),
    JS_CGETSET_MAGIC_DEF("f", nvgjs_transform_get, nvgjs_transform_set, 5),

    JS_CGETSET_MAGIC_DEF("xx", nvgjs_transform_get, nvgjs_transform_set, 0),
    JS_CGETSET_MAGIC_DEF("yx", nvgjs_transform_get, nvgjs_transform_set, 1),
    JS_CGETSET_MAGIC_DEF("xy", nvgjs_transform_get, nvgjs_transform_set, 2),
    JS_CGETSET_MAGIC_DEF("yy", nvgjs_transform_get, nvgjs_transform_set, 3),
    JS_CGETSET_MAGIC_DEF("x0", nvgjs_transform_get, nvgjs_transform_set, 4),
    JS_CGETSET_MAGIC_DEF("y0", nvgjs_transform_get, nvgjs_transform_set, 5),

    JS_CFUNC_MAGIC_DEF("Translate", 2, nvgjs_transform_function, TRANSFORM_TRANSLATE),
    JS_CFUNC_MAGIC_DEF("Scale", 2, nvgjs_transform_function, TRANSFORM_SCALE),
    JS_CFUNC_MAGIC_DEF("Rotate", 1, nvgjs_transform_function, TRANSFORM_ROTATE),
    JS_CFUNC_MAGIC_DEF("SkewX", 1, nvgjs_transform_function, TRANSFORM_SKEWX),
    JS_CFUNC_MAGIC_DEF("SkewY", 1, nvgjs_transform_function, TRANSFORM_SKEWY),
    JS_CFUNC_MAGIC_DEF("Multiply", 1, nvgjs_transform_function, TRANSFORM_MULTIPLY),
    JS_CFUNC_MAGIC_DEF("Premultiply", 1, nvgjs_transform_function, TRANSFORM_PREMULTIPLY),
    JS_CFUNC_MAGIC_DEF("Inverse", 0, nvgjs_transform_function, TRANSFORM_INVERSE),

    JS_CFUNC_MAGIC_DEF("TransformPoint", 1, nvgjs_transform_function, TRANSFORM_POINT),

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

  // GLEW generates GL error because it calls glGetString(GL_EXTENSIONS), we'll
  // consume it here.
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

  NVGLUframebuffer* fb;

  if(!(fb = nvgluCreateFramebuffer(nvg, w, h, imageFlags)))
    return JS_ThrowInternalError(ctx, "Failed creating NVGLUframebuffer [%ix%i] (%i)", w, h, imageFlags);

  return nvgjs_framebuffer_wrap(ctx, framebuffer_proto, fb);
}

NVGJS_DECL(func, BindFramebuffer) {
  NVGLUframebuffer* fb = 0;

  if(!JS_IsNull(argv[0]))
    if(!(fb = JS_GetOpaque2(ctx, argv[0], nvgjs_framebuffer_class_id)))
      return JS_EXCEPTION;

  nvgluBindFramebuffer(fb);

  return JS_UNDEFINED;
}

NVGJS_DECL(func, DeleteFramebuffer) {
  NVGJS_FRAMEBUFFER(argv[0]);

  nvgluDeleteFramebuffer(fb);
  JS_SetOpaque(argv[0], 0);

  return JS_UNDEFINED;
}

NVGJS_DECL(func, ReadPixels) {
  uint32_t w, h;

  JS_ToUint32(ctx, &w, argv[0]);
  JS_ToUint32(ctx, &h, argv[1]);

  void* image;

  /* XXX: better write to a provided ArrayBuffer */
  if(!(image = js_malloc(ctx, w * h * 4)))
    return JS_EXCEPTION;

  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, image);

  return JS_NewArrayBuffer(ctx, image, w * h * 4, (JSFreeArrayBufferDataFunc*)&js_free_rt, image, FALSE);
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

  if(JS_ToInt32(ctx, &r, argv[0]) || JS_ToInt32(ctx, &g, argv[1]) || JS_ToInt32(ctx, &b, argv[2]) ||
     JS_ToInt32(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgRGBA(r, g, b, a));
}

NVGJS_DECL(func, RGBAf) {
  double r, g, b, a;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &r, argv[0]) || JS_ToFloat64(ctx, &g, argv[1]) || JS_ToFloat64(ctx, &b, argv[2]) ||
     JS_ToFloat64(ctx, &a, argv[3]))
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

  if(JS_ToFloat64(ctx, &h, argv[0]) || JS_ToFloat64(ctx, &s, argv[1]) || JS_ToFloat64(ctx, &l, argv[2]) ||
     JS_ToInt32(ctx, &a, argv[3]))
    return JS_EXCEPTION;

  return nvgjs_color_new(ctx, nvgHSLA(h, s, l, a));
}

NVGJS_DECL(func, TransformPoint) {
  float trf[6], *dst, src[2];
  int size, i = 0;

  if(argc < 2)
    return JS_ThrowInternalError(ctx, "need 2 arguments");

  if(!(dst = nvgjs_outputarray(ctx, &size, argv[0])))
    return JS_EXCEPTION;

  nvgjs_inputarray(ctx, trf, 6, argv[1]);

  if(argc > 2) {
    for(; size >= 2; size -= 2, dst += 2, i++) {
      int r;

      if(!(r = nvgjs_arguments(ctx, src, 2, argc - 2, argv + 2)))
        break;

      argc -= r;
      argv += r;

      nvgTransformPoint(&dst[0], &dst[1], trf, src[0], src[1]);
    }

  } else {
    for(; size >= 2; size -= 2, dst += 2, i++)
      nvgTransformPoint(&dst[0], &dst[1], trf, dst[0], dst[1]);
  }

  return JS_NewInt32(ctx, i);
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

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) ||
     JS_ToFloat64(ctx, &h, argv[3]))
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

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &rx, argv[2]) ||
     JS_ToFloat64(ctx, &ry, argv[3]))
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

  if(JS_ToFloat64(ctx, &c1x, argv[0]) || JS_ToFloat64(ctx, &c1y, argv[1]) || JS_ToFloat64(ctx, &c2x, argv[2]) ||
     JS_ToFloat64(ctx, &c2y, argv[3]) || JS_ToFloat64(ctx, &x, argv[4]) || JS_ToFloat64(ctx, &y, argv[5]))
    return JS_EXCEPTION;

  nvgBezierTo(nvg, c1x, c1y, c2x, c2y, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, QuadTo) {
  NVGJS_CONTEXT(this_obj);

  double cx, cy, x, y;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &x, argv[2]) ||
     JS_ToFloat64(ctx, &y, argv[3]))
    return JS_EXCEPTION;

  nvgQuadTo(nvg, cx, cy, x, y);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, ArcTo) {
  NVGJS_CONTEXT(this_obj);

  double x1, y1, x2, y2, radius;

  if(argc < 5)
    return JS_ThrowInternalError(ctx, "need 5 arguments");

  if(JS_ToFloat64(ctx, &x1, argv[0]) || JS_ToFloat64(ctx, &y1, argv[1]) || JS_ToFloat64(ctx, &x2, argv[2]) ||
     JS_ToFloat64(ctx, &y2, argv[3]) || JS_ToFloat64(ctx, &radius, argv[4]))
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

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &r, argv[2]) ||
     JS_ToFloat64(ctx, &a0, argv[3]) || JS_ToFloat64(ctx, &a1, argv[4]) || JS_ToInt32(ctx, &dir, argv[5]))
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

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) ||
     JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &r, argv[4]))
    return JS_EXCEPTION;

  nvgRoundedRect(nvg, x, y, w, h, r);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, RoundedRectVarying) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h, rtl, rtr, rbr, rbl;

  if(argc < 8)
    return JS_ThrowInternalError(ctx, "need 8 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) ||
     JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &rtl, argv[4]) || JS_ToFloat64(ctx, &rtr, argv[5]) ||
     JS_ToFloat64(ctx, &rbr, argv[6]) || JS_ToFloat64(ctx, &rbl, argv[7]))
    return JS_EXCEPTION;

  nvgRoundedRectVarying(nvg, x, y, w, h, rtl, rtr, rbr, rbl);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, Scissor) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) ||
     JS_ToFloat64(ctx, &h, argv[3]))
    return JS_EXCEPTION;

  nvgScissor(nvg, x, y, w, h);
  return JS_UNDEFINED;
}

NVGJS_DECL(Context, IntersectScissor) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h;

  if(argc < 4)
    return JS_ThrowInternalError(ctx, "need 4 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) ||
     JS_ToFloat64(ctx, &h, argv[3]))
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

  if(JS_ToFloat64(ctx, &sx, argv[0]) || JS_ToFloat64(ctx, &sy, argv[1]) || JS_ToFloat64(ctx, &ex, argv[2]) ||
     JS_ToFloat64(ctx, &ey, argv[3]) || nvgjs_tocolor(ctx, &icol, argv[4]) ||
     nvgjs_tocolor(ctx, &ocol, argv[5]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgLinearGradient(nvg, sx, sy, ex, ey, icol, ocol));
}

NVGJS_DECL(Context, BoxGradient) {
  NVGJS_CONTEXT(this_obj);

  double x, y, w, h, r, f;
  NVGcolor icol, ocol;

  if(argc < 8)
    return JS_ThrowInternalError(ctx, "need 8 arguments");

  if(JS_ToFloat64(ctx, &x, argv[0]) || JS_ToFloat64(ctx, &y, argv[1]) || JS_ToFloat64(ctx, &w, argv[2]) ||
     JS_ToFloat64(ctx, &h, argv[3]) || JS_ToFloat64(ctx, &r, argv[4]) || JS_ToFloat64(ctx, &f, argv[5]) ||
     nvgjs_tocolor(ctx, &icol, argv[6]) || nvgjs_tocolor(ctx, &ocol, argv[7]))
    return JS_EXCEPTION;

  return nvgjs_paint_new(ctx, nvgBoxGradient(nvg, x, y, w, h, r, f, icol, ocol));
}

NVGJS_DECL(Context, RadialGradient) {
  NVGJS_CONTEXT(this_obj);

  double cx, cy, inr, outr;
  NVGcolor icol, ocol;

  if(argc < 6)
    return JS_ThrowInternalError(ctx, "need 6 arguments");

  if(JS_ToFloat64(ctx, &cx, argv[0]) || JS_ToFloat64(ctx, &cy, argv[1]) || JS_ToFloat64(ctx, &inr, argv[2]) ||
     JS_ToFloat64(ctx, &outr, argv[3]) || nvgjs_tocolor(ctx, &icol, argv[4]) ||
     nvgjs_tocolor(ctx, &ocol, argv[5]))
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
    int32_t pos;

    if(!JS_ToInt32(ctx, &pos, argv[3]))
      end = str + nvgjs_utf8offset(str, len, pos);
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
    int32_t pos;

    if(!JS_ToInt32(ctx, &pos, argv[4]))
      end = str + nvgjs_utf8offset(str, len, pos);
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
    int32_t pos;

    if(!JS_ToInt32(ctx, &pos, argv[3]))
      end = str + nvgjs_utf8offset(str, len, pos);
  }

  float ret = nvgTextBounds(nvg, x, y, str, end, bounds);

  JS_FreeCString(ctx, str);

  nvgjs_copyobject(
      ctx, argv[4], (const char* const[]){"xmin", "ymin", "xmax", "ymax"}, bounds, countof(bounds));

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
    int32_t pos;

    if(!JS_ToInt32(ctx, &pos, argv[4]))
      end = str + nvgjs_utf8offset(str, len, pos);
  }

  nvgTextBoxBounds(nvg, x, y, breakRowWidth, str, end, bounds);

  JS_FreeCString(ctx, str);

  nvgjs_copyobject(
      ctx, argv[5], (const char* const[]){"xmin", "ymin", "xmax", "ymax"}, bounds, countof(bounds));

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

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1 arguments");

  if(JS_ToInt32(ctx, &id, argv[0]))
    return JS_EXCEPTION;

  nvgImageSize(nvg, id, &width, &height);

  JSValue ret = JS_NewArray(ctx);
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

  if(argc < 1)
    return JS_ThrowInternalError(ctx, "need 1/6 arguments");

  if(nvgjs_arguments(ctx, t, 6, argc, argv))
    nvgTransform(nvg, t[0], t[1], t[2], t[3], t[4], t[5]);

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

  if(JS_ToFloat64(ctx, &ox, argv[0]) || JS_ToFloat64(ctx, &oy, argv[1]) || JS_ToFloat64(ctx, &ex, argv[2]) ||
     JS_ToFloat64(ctx, &ey, argv[3]) || JS_ToFloat64(ctx, &angle, argv[4]) ||
     JS_ToInt32(ctx, &image, argv[5]) || JS_ToFloat64(ctx, &alpha, argv[6]))
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

    NVGJS_FUNC(ReadPixels, 2),

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

    NVGJS_CONST(PI),
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

enum {
  FRAMEBUFFER_FBO,
  FRAMEBUFFER_RBO,
  FRAMEBUFFER_TEXTURE,
  FRAMEBUFFER_IMAGE,
};

static JSValue
nvgjs_framebuffer_get(JSContext* ctx, JSValueConst this_val, int magic) {
  NVGJS_FRAMEBUFFER(this_val);

  int64_t id;

  switch(magic) {
    case FRAMEBUFFER_FBO: id = fb->fbo; break;
    case FRAMEBUFFER_RBO: id = fb->rbo; break;
    case FRAMEBUFFER_TEXTURE: id = fb->texture; break;
    case FRAMEBUFFER_IMAGE: id = fb->image; break;
  }

  return JS_NewInt64(ctx, id);
}

static JSValue
nvgjs_framebuffer_wrap(JSContext* ctx, JSValueConst proto, NVGLUframebuffer* fb) {
  JSValue obj = JS_NewObjectProtoClass(ctx, proto, nvgjs_framebuffer_class_id);

  if(!JS_IsException(obj))
    JS_SetOpaque(obj, fb);

  return obj;
}

static const JSCFunctionListEntry nvgjs_framebuffer_methods[] = {
    JS_CGETSET_MAGIC_DEF("fbo", nvgjs_framebuffer_get, 0, FRAMEBUFFER_FBO),
    JS_CGETSET_MAGIC_DEF("rbo", nvgjs_framebuffer_get, 0, FRAMEBUFFER_RBO),
    JS_CGETSET_MAGIC_DEF("texture", nvgjs_framebuffer_get, 0, FRAMEBUFFER_TEXTURE),
    JS_CGETSET_MAGIC_DEF("image", nvgjs_framebuffer_get, 0, FRAMEBUFFER_IMAGE),
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
  JS_SetPropertyFunctionList(ctx,
                             transform_ctor,
                             nvgjs_transform_functions,
                             countof(nvgjs_transform_functions));
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
  JS_SetPropertyFunctionList(ctx,
                             framebuffer_proto,
                             nvgjs_framebuffer_methods,
                             countof(nvgjs_framebuffer_methods));

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
