project(qjs-nanovg C)

message("Building project [1;31m${PROJECT_NAME}[0m")

set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH TRUE)

include(FindOpenGL)
if(NOT PKG_CONFIG_FOUND)
  include(FindPkgConfig)
endif(NOT PKG_CONFIG_FOUND)
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

include_directories(${GLEW_INCLUDE_DIR} ${GLFW_INCLUDE_DIR}
                    ${QUICKJS_INCLUDE_DIR})
link_directories(${GLEW_LIBRARY_DIR} ${GLFW_LIBRARY_DIR} ${QUICKJS_LIBRARY_DIR})

add_library(
  qjs-nanovg SHARED
  nanovg-qjs.c nanovg-qjs.h ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.c
  ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.h
  ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg_gl.h)

set_target_properties(
  qjs-nanovg
  PROPERTIES OUTPUT_NAME nanovg PREFIX ""
             COMPILE_FLAGS "${MODULE_COMPILE_FLAGS} -DJS_SHARED_LIBRARY")

target_include_directories(
  qjs-nanovg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src
                     ${QUICKJS_INCLUDE_DIR})
target_link_libraries(qjs-nanovg PRIVATE #${GLFW_LIBRARY}
                                         ${GLEW_LIBRARY}
                                         OpenGL::GL ${QUICKJS_LIBRARY})
target_compile_definitions(qjs-nanovg PRIVATE JS_SHARED_LIBRARY=1 NANOVG_GLEW=1
                                                NANOVG_GL3_IMPLEMENTATION=1
                                              )

if(NOT CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR lib)
endif(NOT CMAKE_INSTALL_LIBDIR)
if(NOT QUICKJS_C_MODULE_DIR)
  set(QUICKJS_C_MODULE_DIR "${CMAKE_INSTALL_LIBDIR}/quickjs")
endif(NOT QUICKJS_C_MODULE_DIR)

install(TARGETS qjs-nanovg DESTINATION "${QUICKJS_C_MODULE_DIR}"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
                    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

include_directories(nanovg/src)
if(BUILD_EXAMPLE)
  add_executable(
    nanovg_example
    nanovg/example/example_gl3.c
    nanovg/example/perf.c
    nanovg/example/demo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src/nanovg_gl.h)
  target_link_libraries(nanovg_example m #${GLFW_LIBRARY}
                        ${GLEW_LIBRARY}
                        OpenGL::GL)

  add_custom_target(run_example COMMAND ${CMAKE_CURRENT_BINARY_DIR}/nanovg_example
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/example)
endif(BUILD_EXAMPLE)
