# TODO

## Unbound public API functions

These NanoVG public API functions are not yet bound in `nvgjs-module.c`:

- nvgDebugDumpPathCache
- nvgGlobalCompositeBlendFunc
- nvgGlobalCompositeBlendFuncSeparate
- nvgGlobalCompositeOperation

## Web build

- Load web/nanovg-web.js in a browser (web/index.html) and check that it
  renders; BUILD_WEB was only verified up to the produced files.
- Decide whether the README option table should list BUILD_SHARED and
  BUILD_STATIC (from `quickjs_module_options()`) instead of
  `BUILD_STATIC_MODULES`, which CMakeLists.txt no longer defines.
- js-engines (`~/Sources/js-engines`): add `PROJ_nanovg__BUILD_WEB` and
  `BUILD_EXAMPLE` to its menuconfig (noted in its TODO.md).
