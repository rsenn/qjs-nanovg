# Find GLFW library and include paths for CMU462
# This defines the following:
#
# GLFW_FOUND             If GLFW is found
# GLFW_LIBRARY           GLFW libraries
# GLFW_INCLUDE_DIR       GLFW include directories
# GLFW_LIBRARY_DIR       GLFW library directories
macro(find_glfw)
  if(UNIX)
    set(GLFW_INC_NAMES glfw.h)
    set(GLFW_LIB_NAMES libglfw.a)
    if(APPLE)
      set(GLFW_LIB_NAMES libglfw_osx.a)
    endif(APPLE)
  endif(UNIX)

  # GLFW static library
  find_library(GLFW_LIBRARY NAMES ${GLFW_LIB_NAMES} PATHS ${PROJECT_SOURCE_DIR}/../lib DOC "GLFW library")

  # GLFW library dir
  find_path(GLFW_LIBRARY_DIR NAMES ${GLFW_LIB_NAMES} PATHS ${PROJECT_SOURCE_DIR}/../lib DOC "462 include directories")

  # GLFW include dir
  find_path(GLFW_INCLUDE_DIR NAMES ${GLFW_INC_NAMES} PATHS ${PROJECT_SOURCE_DIR}/../include/GLFW DOC "462 include directories")

  # Version
  set(GLFW_VERSION 3.1.1)

  # Set package standard args
  #include(FindPackageHandleStandardArgs)
  #FIND_PACKAGE_HANDLE_STANDARD_ARGS(GLFW REQUIRED_VARS GLFW_LIBRARY GLFW_INCLUDE_DIR GLFW_LIBRARY_DIR VERSION_VAR GLFW_VERSION)
endmacro(find_glfw)
