project(qjs-nanovg C)

include(FindOpenGL)
include(FindPkgConfig)
include(CheckCCompilerFlag)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindQuickJS.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGLEW.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGLFW.cmake)

check_c_compiler_flag("-fvisibility=hidden" VISIBILITY_HIDDEN)
if(VISIBILITY_HIDDEN)
  set(MODULE_COMPILE_FLAGS ${MODULE_COMPILE_FLAGS} "-fvisibility=hidden")
endif(VISIBILITY_HIDDEN)

find_glew()
find_glfw()
#[[pkg_search_module(GLEW glew)
pkg_search_module(GLFW glfw3)
set(GLEW_LIBRARY "${pkgcfg_lib_GLEW_glew32}")
set(GLFW_LIBRARY "${pkgcfg_lib_GLFW_glfw3}")
message(STATUS "GLEW_LIBRARY: ${GLEW_LIBRARY}")
message(STATUS "GLFW_LIBRARY: ${GLFW_LIBRARY}")
]]
find_quickjs()

add_library(
  qjs-nanovg SHARED nanovg-qjs.c nanovg-qjs.h ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.c
                    ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.h ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg_gl.h)

set_target_properties(qjs-nanovg PROPERTIES OUTPUT_NAME nanovg PREFIX "" COMPILE_FLAGS "${MODULE_COMPILE_FLAGS}")

target_include_directories(qjs-nanovg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src ${QUICKJS_INCLUDE_DIR})
target_link_libraries(qjs-nanovg PRIVATE ${GLFW_LIBRARY} ${GLEW_LIBRARY} OpenGL::GL ${QUICKJS_LIBRARY})
target_compile_definitions(qjs-nanovg PRIVATE JS_SHARED_LIBRARY=1 NANOVG_GLEW=1 NANOVG_GL3_IMPLEMENTATION=1)
install(TARGETS qjs-nanovg DESTINATION lib/quickjs PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
                                                               GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
