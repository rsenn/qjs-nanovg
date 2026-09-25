#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#define NANOVG_GLES3_IMPLEMENTATION
#include "../nanovg/src/nanovg.h"
#include "../nanovg/src/nanovg_gl.h"
#include "../nanovg/src/nanovg_gl_utils.h"

#define EXPORT EMSCRIPTEN_KEEPALIVE
#define CTX NVGcontext* c

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl;
static NVGpaint paint;
static float xform[6];
static float bounds[4];

#define ENDPTR(str, end) ((end) < 0 ? NULL : (str) + (end))
#define COLOR(n) nvgRGBAf(n##r, n##g, n##b, n##a)

EXPORT NVGcontext* nvgw_Create(int flags) {
  if(!gl) {
    EmscriptenWebGLContextAttributes a;
    emscripten_webgl_init_context_attributes(&a);
    a.majorVersion = 2;
    a.stencil = 1;
    a.antialias = 0;
    if((gl = emscripten_webgl_create_context("#canvas", &a)) <= 0) {
      gl = 0;
      return NULL;
    }
    emscripten_webgl_make_context_current(gl);
  }
  return nvgCreateGLES3(flags);
}

EXPORT void nvgw_Delete(CTX) { nvgDeleteGLES3(c); }

EXPORT int nvgw_CreateImageFromHandle(CTX, unsigned tex, int w, int h, int flags) { return nvglCreateImageFromHandleGLES3(c, tex, w, h, flags); }
EXPORT unsigned nvgw_ImageHandle(CTX, int image) { return nvglImageHandleGLES3(c, image); }

EXPORT NVGLUframebuffer* nvgw_CreateFramebuffer(CTX, int w, int h, int flags) { return nvgluCreateFramebuffer(c, w, h, flags); }
EXPORT void nvgw_BindFramebuffer(NVGLUframebuffer* fb) { nvgluBindFramebuffer(fb); }
EXPORT void nvgw_DeleteFramebuffer(NVGLUframebuffer* fb) { nvgluDeleteFramebuffer(fb); }
EXPORT unsigned nvgw_FramebufferField(NVGLUframebuffer* fb, int which) {
  switch(which) {
    case 0: return fb->fbo;
    case 1: return fb->rbo;
    case 2: return fb->texture;
    default: return fb->image;
  }
}
EXPORT void nvgw_ReadPixels(int w, int h, void* dst) { glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dst); }

EXPORT void nvgw_BeginFrame(CTX, float w, float h, float ratio) { nvgBeginFrame(c, w, h, ratio); }
EXPORT void nvgw_CancelFrame(CTX) { nvgCancelFrame(c); }
EXPORT void nvgw_EndFrame(CTX) { nvgEndFrame(c); }
EXPORT void nvgw_Save(CTX) { nvgSave(c); }
EXPORT void nvgw_Restore(CTX) { nvgRestore(c); }
EXPORT void nvgw_Reset(CTX) { nvgReset(c); }
EXPORT void nvgw_ShapeAntiAlias(CTX, int on) { nvgShapeAntiAlias(c, on); }

EXPORT void nvgw_Scissor(CTX, float x, float y, float w, float h) { nvgScissor(c, x, y, w, h); }
EXPORT void nvgw_IntersectScissor(CTX, float x, float y, float w, float h) { nvgIntersectScissor(c, x, y, w, h); }
EXPORT void nvgw_ResetScissor(CTX) { nvgResetScissor(c); }

EXPORT void nvgw_MiterLimit(CTX, float v) { nvgMiterLimit(c, v); }
EXPORT void nvgw_LineCap(CTX, int v) { nvgLineCap(c, v); }
EXPORT void nvgw_LineJoin(CTX, int v) { nvgLineJoin(c, v); }
EXPORT void nvgw_GlobalAlpha(CTX, float v) { nvgGlobalAlpha(c, v); }
EXPORT void nvgw_StrokeWidth(CTX, float v) { nvgStrokeWidth(c, v); }
EXPORT void nvgw_StrokeColor(CTX, float r, float g, float b, float a) { nvgStrokeColor(c, nvgRGBAf(r, g, b, a)); }
EXPORT void nvgw_FillColor(CTX, float r, float g, float b, float a) { nvgFillColor(c, nvgRGBAf(r, g, b, a)); }
EXPORT void nvgw_StrokePaint(CTX) { nvgStrokePaint(c, paint); }
EXPORT void nvgw_FillPaint(CTX) { nvgFillPaint(c, paint); }

EXPORT void* nvgw_PaintPtr(void) { return &paint; }
EXPORT int nvgw_PaintSize(void) { return sizeof(paint); }
EXPORT void nvgw_LinearGradient(CTX, float sx, float sy, float ex, float ey, float ir, float ig, float ib, float ia, float or, float og, float ob, float oa) {
  paint = nvgLinearGradient(c, sx, sy, ex, ey, COLOR(i), COLOR(o));
}
EXPORT void nvgw_BoxGradient(CTX, float x, float y, float w, float h, float r, float f, float ir, float ig, float ib, float ia, float or, float og, float ob, float oa) {
  paint = nvgBoxGradient(c, x, y, w, h, r, f, COLOR(i), COLOR(o));
}
EXPORT void nvgw_RadialGradient(CTX, float cx, float cy, float inr, float outr, float ir, float ig, float ib, float ia, float or, float og, float ob, float oa) {
  paint = nvgRadialGradient(c, cx, cy, inr, outr, COLOR(i), COLOR(o));
}
EXPORT void nvgw_ImagePattern(CTX, float ox, float oy, float ex, float ey, float angle, int image, float alpha) {
  paint = nvgImagePattern(c, ox, oy, ex, ey, angle, image, alpha);
}

EXPORT void nvgw_ResetTransform(CTX) { nvgResetTransform(c); }
EXPORT void nvgw_Transform(CTX, float a, float b, float cc, float d, float e, float f) { nvgTransform(c, a, b, cc, d, e, f); }
EXPORT void nvgw_Translate(CTX, float x, float y) { nvgTranslate(c, x, y); }
EXPORT void nvgw_Rotate(CTX, float a) { nvgRotate(c, a); }
EXPORT void nvgw_SkewX(CTX, float a) { nvgSkewX(c, a); }
EXPORT void nvgw_SkewY(CTX, float a) { nvgSkewY(c, a); }
EXPORT void nvgw_Scale(CTX, float x, float y) { nvgScale(c, x, y); }
EXPORT float* nvgw_CurrentTransform(CTX) {
  nvgCurrentTransform(c, xform);
  return xform;
}

EXPORT void nvgw_BeginPath(CTX) { nvgBeginPath(c); }
EXPORT void nvgw_ClosePath(CTX) { nvgClosePath(c); }
EXPORT void nvgw_PathWinding(CTX, int dir) { nvgPathWinding(c, dir); }
EXPORT void nvgw_MoveTo(CTX, float x, float y) { nvgMoveTo(c, x, y); }
EXPORT void nvgw_LineTo(CTX, float x, float y) { nvgLineTo(c, x, y); }
EXPORT void nvgw_BezierTo(CTX, float a, float b, float cc, float d, float x, float y) { nvgBezierTo(c, a, b, cc, d, x, y); }
EXPORT void nvgw_QuadTo(CTX, float cx, float cy, float x, float y) { nvgQuadTo(c, cx, cy, x, y); }
EXPORT void nvgw_ArcTo(CTX, float x1, float y1, float x2, float y2, float r) { nvgArcTo(c, x1, y1, x2, y2, r); }
EXPORT void nvgw_Arc(CTX, float cx, float cy, float r, float a0, float a1, int dir) { nvgArc(c, cx, cy, r, a0, a1, dir); }
EXPORT void nvgw_Rect(CTX, float x, float y, float w, float h) { nvgRect(c, x, y, w, h); }
EXPORT void nvgw_RoundedRect(CTX, float x, float y, float w, float h, float r) { nvgRoundedRect(c, x, y, w, h, r); }
EXPORT void nvgw_RoundedRectVarying(CTX, float x, float y, float w, float h, float tl, float tr, float br, float bl) { nvgRoundedRectVarying(c, x, y, w, h, tl, tr, br, bl); }
EXPORT void nvgw_Circle(CTX, float x, float y, float r) { nvgCircle(c, x, y, r); }
EXPORT void nvgw_Ellipse(CTX, float x, float y, float rx, float ry) { nvgEllipse(c, x, y, rx, ry); }
EXPORT void nvgw_Fill(CTX) { nvgFill(c); }
EXPORT void nvgw_Stroke(CTX) { nvgStroke(c); }

EXPORT int nvgw_CreateFont(CTX, const char* name, const char* file) { return nvgCreateFont(c, name, file); }
EXPORT int nvgw_CreateFontAtIndex(CTX, const char* name, const char* file, int index) { return nvgCreateFontAtIndex(c, name, file, index); }
EXPORT int nvgw_FindFont(CTX, const char* name) { return nvgFindFont(c, name); }
EXPORT void nvgw_FontFace(CTX, const char* name) { nvgFontFace(c, name); }
EXPORT void nvgw_FontSize(CTX, float v) { nvgFontSize(c, v); }
EXPORT void nvgw_FontBlur(CTX, float v) { nvgFontBlur(c, v); }
EXPORT void nvgw_TextLetterSpacing(CTX, float v) { nvgTextLetterSpacing(c, v); }
EXPORT void nvgw_TextLineHeight(CTX, float v) { nvgTextLineHeight(c, v); }
EXPORT void nvgw_TextAlign(CTX, int v) { nvgTextAlign(c, v); }

EXPORT float nvgw_Text(CTX, float x, float y, const char* str, int end) { return nvgText(c, x, y, str, ENDPTR(str, end)); }
EXPORT void nvgw_TextBox(CTX, float x, float y, float width, const char* str, int end) { nvgTextBox(c, x, y, width, str, ENDPTR(str, end)); }
EXPORT float* nvgw_TextBounds(CTX, float x, float y, const char* str, int end, float* advance) {
  *advance = nvgTextBounds(c, x, y, str, ENDPTR(str, end), bounds);
  return bounds;
}
EXPORT float* nvgw_TextBoxBounds(CTX, float x, float y, float width, const char* str, int end) {
  nvgTextBoxBounds(c, x, y, width, str, ENDPTR(str, end), bounds);
  return bounds;
}

EXPORT int nvgw_CreateImage(CTX, const char* file, int flags) { return nvgCreateImage(c, file, flags); }
EXPORT int nvgw_CreateImageMem(CTX, int flags, unsigned char* data, int n) { return nvgCreateImageMem(c, flags, data, n); }
EXPORT int nvgw_CreateImageRGBA(CTX, int w, int h, int flags, const unsigned char* data) { return nvgCreateImageRGBA(c, w, h, flags, data); }
EXPORT void nvgw_UpdateImage(CTX, int image, const unsigned char* data) { nvgUpdateImage(c, image, data); }
EXPORT int nvgw_ImageWidth(CTX, int image) {
  int w, h;
  nvgImageSize(c, image, &w, &h);
  return w;
}
EXPORT int nvgw_ImageHeight(CTX, int image) {
  int w, h;
  nvgImageSize(c, image, &w, &h);
  return h;
}
EXPORT void nvgw_DeleteImage(CTX, int image) { nvgDeleteImage(c, image); }
