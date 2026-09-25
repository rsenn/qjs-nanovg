#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#define NANOVG_GLES3_IMPLEMENTATION
#include "../nanovg/src/nanovg.h"
#include "../nanovg/src/nanovg_gl.h"

static NVGcontext* vg;
static NVGpaint paint;
static float xform[6];

#define EXPORT EMSCRIPTEN_KEEPALIVE

EXPORT int nvgw_Create(int flags) {
  EmscriptenWebGLContextAttributes a;
  emscripten_webgl_init_context_attributes(&a);
  a.majorVersion = 2;
  a.stencil = 1;
  a.antialias = 0;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE c = emscripten_webgl_create_context("#canvas", &a);
  if(c <= 0)
    return 0;
  emscripten_webgl_make_context_current(c);
  vg = nvgCreateGLES3(flags);
  return vg != NULL;
}

EXPORT void nvgw_Delete(void) {
  nvgDeleteGLES3(vg);
  vg = NULL;
}

EXPORT void nvgw_BeginFrame(int w, int h, float ratio) {
  glViewport(0, 0, w, h);
  nvgBeginFrame(vg, w, h, ratio);
}
EXPORT void nvgw_EndFrame(void) { nvgEndFrame(vg); }
EXPORT void nvgw_Save(void) { nvgSave(vg); }
EXPORT void nvgw_Restore(void) { nvgRestore(vg); }

EXPORT void nvgw_BeginPath(void) { nvgBeginPath(vg); }
EXPORT void nvgw_MoveTo(float x, float y) { nvgMoveTo(vg, x, y); }
EXPORT void nvgw_LineTo(float x, float y) { nvgLineTo(vg, x, y); }
EXPORT void nvgw_BezierTo(float a, float b, float c, float d, float x, float y) { nvgBezierTo(vg, a, b, c, d, x, y); }
EXPORT void nvgw_QuadTo(float cx, float cy, float x, float y) { nvgQuadTo(vg, cx, cy, x, y); }
EXPORT void nvgw_Rect(float x, float y, float w, float h) { nvgRect(vg, x, y, w, h); }
EXPORT void nvgw_RoundedRect(float x, float y, float w, float h, float r) { nvgRoundedRect(vg, x, y, w, h, r); }
EXPORT void nvgw_Circle(float x, float y, float r) { nvgCircle(vg, x, y, r); }
EXPORT void nvgw_Ellipse(float x, float y, float rx, float ry) { nvgEllipse(vg, x, y, rx, ry); }
EXPORT void nvgw_Fill(void) { nvgFill(vg); }
EXPORT void nvgw_Stroke(void) { nvgStroke(vg); }

EXPORT void nvgw_FillColor(float r, float g, float b, float a) { nvgFillColor(vg, nvgRGBAf(r, g, b, a)); }
EXPORT void nvgw_StrokeColor(float r, float g, float b, float a) { nvgStrokeColor(vg, nvgRGBAf(r, g, b, a)); }
EXPORT void nvgw_StrokeWidth(float w) { nvgStrokeWidth(vg, w); }

EXPORT void nvgw_Translate(float x, float y) { nvgTranslate(vg, x, y); }
EXPORT void nvgw_Rotate(float a) { nvgRotate(vg, a); }
EXPORT void nvgw_Scale(float x, float y) { nvgScale(vg, x, y); }
EXPORT float* nvgw_CurrentTransform(void) {
  nvgCurrentTransform(vg, xform);
  return xform;
}

EXPORT void nvgw_ImagePattern(float ox, float oy, float ex, float ey, float angle, int image, float alpha) {
  paint = nvgImagePattern(vg, ox, oy, ex, ey, angle, image, alpha);
}
EXPORT void* nvgw_PaintPtr(void) { return &paint; }
EXPORT int nvgw_PaintSize(void) { return sizeof(paint); }
EXPORT void nvgw_FillPaint(void) { nvgFillPaint(vg, paint); }

EXPORT int nvgw_CreateImageRGBA(int w, int h, int flags, const unsigned char* data) { return nvgCreateImageRGBA(vg, w, h, flags, data); }
EXPORT int nvgw_ImageWidth(int id) {
  int w, h;
  nvgImageSize(vg, id, &w, &h);
  return w;
}
EXPORT int nvgw_ImageHeight(int id) {
  int w, h;
  nvgImageSize(vg, id, &w, &h);
  return h;
}

EXPORT int nvgw_CreateFontMem(const char* name, unsigned char* data, int n) { return nvgCreateFontMem(vg, name, data, n, 1); }
EXPORT void nvgw_FontFace(const char* name) { nvgFontFace(vg, name); }
EXPORT void nvgw_FontSize(float size) { nvgFontSize(vg, size); }
EXPORT void nvgw_TextAlign(int align) { nvgTextAlign(vg, align); }
EXPORT float nvgw_Text(float x, float y, const char* str) { return nvgText(vg, x, y, str, NULL); }
