project(qjs-nanovg C)


include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindQuickJS.cmake)
include(FindOpenGL)
include(FindGLEW)
include(FindPkgConfig)

pkg_search_module(GLFW3 glfw3)
message(STATUS "GLFW3_LIBRARY: ${GLFW3_LIBRARY}")

find_quickjs()

add_library(
  qjs-nanovg SHARED
  nanovg-qjs.c nanovg-qjs.h ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.c
  ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.h
  ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg_gl.h)

set_target_properties(qjs-nanovg PROPERTIES OUTPUT_NAME nanovg PREFIX "")

target_include_directories(qjs-nanovg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src
                                              ${QUICKJS_INCLUDE_DIR})
target_link_libraries(qjs-nanovg PRIVATE GLEW::glew OpenGL::GL ${QUICKJS_LIBRARY})
target_compile_definitions(qjs-nanovg PRIVATE JS_SHARED_LIBRARY=1 NANOVG_GLEW=1
                                              NANOVG_GL3_IMPLEMENTATION=1)
install(TARGETS qjs-nanovg DESTINATION lib/quickjs
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ
                    WORLD_EXECUTE)
