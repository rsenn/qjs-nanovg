# Build GLEW from source when it is not installed for the target.
#
# build_glew() downloads the GLEW release tarball (or uses GLEW_SOURCE_DIR), configures it
# with the current toolchain (native or cross), builds the static library and installs it to
# ${GLEW_BUILD_PREFIX}. Sets GLEW_BUILT_STATIC, so consumers must define GLEW_STATIC, and
# leaves GLEW_LIBRARY, GLEW_LIBRARY_DIR and GLEW_INCLUDE_DIR as plain paths for find_glew().
#
# Variables: GLEW_VERSION_BUILD (default 2.2.0), GLEW_URL, GLEW_SOURCE_DIR, GLEW_BUILD_PREFIX

set(GLEW_VERSION_BUILD "2.2.0" CACHE STRING "GLEW version fetched by build_glew()")

macro(build_glew)
  set(GLEW_BUILD_PREFIX "${CMAKE_BINARY_DIR}/glew-install" CACHE PATH "where build_glew() installs GLEW")
  set(_glew_work "${CMAKE_BINARY_DIR}/glew-build")

  if(NOT GLEW_SOURCE_DIR)
    set(_glew_name "glew-${GLEW_VERSION_BUILD}")
    if(NOT GLEW_URL)
      set(GLEW_URL "https://github.com/nigels-com/glew/releases/download/${_glew_name}/${_glew_name}.tgz")
    endif(NOT GLEW_URL)
    set(_glew_tgz "${_glew_work}/${_glew_name}.tgz")
    if(NOT EXISTS "${_glew_work}/${_glew_name}/build/cmake/CMakeLists.txt")
      message(STATUS "Downloading GLEW: ${GLEW_URL}")
      file(DOWNLOAD "${GLEW_URL}" "${_glew_tgz}" STATUS _glew_status TLS_VERIFY ON)
      list(GET _glew_status 0 _glew_rc)
      if(NOT _glew_rc EQUAL 0)
        message(FATAL_ERROR "Downloading GLEW failed: ${_glew_status} (set GLEW_SOURCE_DIR to a source tree)")
      endif(NOT _glew_rc EQUAL 0)
      file(ARCHIVE_EXTRACT INPUT "${_glew_tgz}" DESTINATION "${_glew_work}")
    endif(NOT EXISTS "${_glew_work}/${_glew_name}/build/cmake/CMakeLists.txt")
    set(GLEW_SOURCE_DIR "${_glew_work}/${_glew_name}")
  endif(NOT GLEW_SOURCE_DIR)

  set(_glew_args
      -DCMAKE_INSTALL_PREFIX=${GLEW_BUILD_PREFIX} -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
      -DBUILD_UTILS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_LIBDIR=lib)
  if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND _glew_args -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
  endif(CMAKE_TOOLCHAIN_FILE)

  message(STATUS "Building GLEW in ${_glew_work}/b")
  foreach(_glew_step "" "--build" "--install")
    if(_glew_step STREQUAL "")
      set(_glew_cmd ${CMAKE_COMMAND} -S "${GLEW_SOURCE_DIR}/build/cmake" -B "${_glew_work}/b" ${_glew_args})
    else()
      set(_glew_cmd ${CMAKE_COMMAND} ${_glew_step} "${_glew_work}/b")
    endif()
    execute_process(COMMAND ${_glew_cmd} RESULT_VARIABLE _glew_rc OUTPUT_VARIABLE _glew_out ERROR_VARIABLE _glew_out)
    if(NOT _glew_rc EQUAL 0)
      message(FATAL_ERROR "Building GLEW failed (${_glew_step}):\n${_glew_out}")
    endif(NOT _glew_rc EQUAL 0)
  endforeach(_glew_step)

  unset(GLEW_LIBRARY CACHE)
  find_library(GLEW_LIBRARY NAMES GLEW glew32 libglew32 PATHS "${GLEW_BUILD_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  if(NOT GLEW_LIBRARY)
    message(FATAL_ERROR "GLEW was built but no library found in ${GLEW_BUILD_PREFIX}/lib")
  endif(NOT GLEW_LIBRARY)
  set(GLEW_LIBRARY_DIR "${GLEW_BUILD_PREFIX}/lib")
  set(GLEW_INCLUDE_DIR "${GLEW_BUILD_PREFIX}/include")
  set(GLEW_BUILT_STATIC TRUE CACHE INTERNAL "GLEW was built from source as a static library")
endmacro(build_glew)
