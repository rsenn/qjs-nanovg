cmake_minimum_required(VERSION 3.5)

project(qjs-nanovg VERSION 0.9 LANGUAGES C)

message("Building project [1;31m${PROJECT_NAME}[0m")

set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH TRUE)

include(FindOpenGL)
if(NOT PKG_CONFIG_FOUND)
  include(FindPkgConfig)
endif(NOT PKG_CONFIG_FOUND)

include(CheckCCompilerFlag)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Functions.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Compat.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindQuickJS.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/QuickJSModule.cmake)
quickjs_module_options(SHARED_DEFAULT ON STATIC_DEFAULT ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGLEW.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGLFW.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UseMultiArch.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindMarkdown.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/markdown.cmake)

check_c_compiler_flag("-fvisibility=hidden" VISIBILITY_HIDDEN)
if(VISIBILITY_HIDDEN)
  set(MODULE_COMPILE_FLAGS ${MODULE_COMPILE_FLAGS} "-fvisibility=hidden")
endif(VISIBILITY_HIDDEN)

find_glew()
find_glfw()

find_quickjs()
configure_quickjs()

include_directories(${GLEW_INCLUDE_DIR} ${GLFW_INCLUDE_DIR} ${QUICKJS_INCLUDE_DIR})
link_directories(${GLEW_LIBRARY_DIR} ${GLFW_LIBRARY_DIR} ${QUICKJS_LIBRARY_DIR})

# qjs-nanovg (nanovg.so, BUILD_SHARED) and qjs-nanovg-static (quickjs-nanovg.a plus
# quickjs-nanovg.module.cmake, BUILD_STATIC) from make_module().
set(nanovg_LIBRARIES ${GLEW_LIBRARY} OpenGL::GL)
# the static module's link line is exported to other configures: plain paths, no imported targets
set(nanovg_LIBRARIES_static ${GLEW_LIBRARY} ${OPENGL_LIBRARIES})

make_module(
  nanovg
  nvgjs-utils.h nvgjs-utils.c nvgjs-module.h nvgjs-module.c
  nanovg/src/nanovg.c
  nanovg/src/nanovg.h
  nanovg/src/nanovg_gl.h)

foreach(_t qjs-nanovg qjs-nanovg-static)
  if(TARGET ${_t})
    target_include_directories(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src ${QUICKJS_INCLUDE_DIR})
    target_compile_definitions(${_t} PRIVATE NANOVG_GLEW=1 NANOVG_GL3_IMPLEMENTATION=1)
  endif(TARGET ${_t})
endforeach(_t)

install(FILES lib/canvas2d.js DESTINATION "${QUICKJS_C_MODULE_DIR}")

# Browser build of the same bindings (NanoVG on WebGL2 through Emscripten). The output lands in
# web/ beside the JS API that wraps it, so web/ is runnable straight after a normal build.
option(BUILD_WEB "Also build web/nanovg-web.js and web/nanovg-web.wasm with Emscripten" ON)
if(BUILD_WEB)
  find_program(EMCC emcc HINTS "$ENV{EMSDK}/upstream/emscripten" "$ENV{EMSCRIPTEN}")

  if(EMCC)
    set(WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/web)
    set(WEB_OUTPUT ${WEB_DIR}/nanovg-web.js ${WEB_DIR}/nanovg-web.wasm)

    add_custom_command(
      OUTPUT ${WEB_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E env EMCC=${EMCC} sh ${WEB_DIR}/build.sh
      DEPENDS ${WEB_DIR}/nanovg-web.c ${WEB_DIR}/build.sh nanovg/src/nanovg.c nanovg/src/nanovg.h
              nanovg/src/nanovg_gl.h nanovg/src/nanovg_gl_utils.h
      COMMENT "Building web/nanovg-web.js with Emscripten"
      VERBATIM)
    add_custom_target(qjs-nanovg-web ALL DEPENDS ${WEB_OUTPUT})
  else(EMCC)
    message(STATUS "emcc not found, skipping the web build (use -DEMCC=/path/to/emcc, or set EMSDK)")
  endif(EMCC)
endif(BUILD_WEB)

include_directories(nanovg/src)
if(BUILD_EXAMPLE)
  add_executable(
    nanovg_example
    nanovg/example/example_gl3.c nanovg/example/perf.c nanovg/example/demo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.c ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg_gl.h)
  target_link_libraries(nanovg_example m #${GLFW_LIBRARY}
                        ${GLEW_LIBRARY} OpenGL::GL)

  add_custom_target(run_example COMMAND ${CMAKE_CURRENT_BINARY_DIR}/nanovg_example
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/example)
endif(BUILD_EXAMPLE)

file(GLOB MARKDOWN_DOCS doc/*.md)
md2html(${MARKDOWN_DOCS})
