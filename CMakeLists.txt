project(qjs-nanovg C)

include(FindOpenGL)
include(FindPkgConfig)
include(CheckCCompilerFlag)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindQuickJS.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGLEW.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGLFW.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UseMultiArch.cmake)

check_c_compiler_flag("-fvisibility=hidden" VISIBILITY_HIDDEN)
if(VISIBILITY_HIDDEN)
  set(MODULE_COMPILE_FLAGS ${MODULE_COMPILE_FLAGS} "-fvisibility=hidden")
endif(VISIBILITY_HIDDEN)

find_glew()
find_glfw()

find_quickjs()
configure_quickjs()

add_library(
  qjs-nanovg SHARED nanovg-qjs.c nanovg-qjs.h ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.c
                    ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.h ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg_gl.h)

set_target_properties(qjs-nanovg PROPERTIES OUTPUT_NAME nanovg PREFIX "" COMPILE_FLAGS "${MODULE_COMPILE_FLAGS}")

target_include_directories(qjs-nanovg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src ${QUICKJS_INCLUDE_DIR})
target_link_libraries(qjs-nanovg PRIVATE ${GLFW_LIBRARY} ${GLEW_LIBRARY} OpenGL::GL ${QUICKJS_LIBRARY})
target_compile_definitions(qjs-nanovg PRIVATE JS_SHARED_LIBRARY=1 NANOVG_GLEW=1 NANOVG_GL3_IMPLEMENTATION=1)

if(NOT CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR lib)
endif(NOT CMAKE_INSTALL_LIBDIR)
if(NOT QUICKJS_C_MODULE_DIR)
  set(QUICKJS_C_MODULE_DIR "${CMAKE_INSTALL_LIBDIR}/quickjs")
endif(NOT QUICKJS_C_MODULE_DIR)

install(TARGETS qjs-nanovg DESTINATION "${QUICKJS_C_MODULE_DIR}"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
