# Find GLEW library and include paths for CMU462
# This defines the following:
#
# GLEW_FOUND             If GLEW is found
# GLEW_LIBRARY           GLEW libraries
# GLEW_INCLUDE_DIR       GLEW include directories
# GLEW_LIBRARY_DIR       GLEW library directories
macro(find_glew)
  if(UNIX)
    set(GLEW_INC_NAMES glew.h)
    set(GLEW_LIB_NAMES libglew.a)
    if(APPLE)
      set(GLEW_LIB_NAMES libglew_osx.a)
    endif(APPLE)
  endif(UNIX)

  # GLEW static library
  find_library(GLEW_LIBRARY NAMES ${GLEW_LIB_NAMES} PATHS ${PROJECT_SOURCE_DIR}/../lib DOC "GLEW library")

  # GLEW library dir
  find_path(GLEW_LIBRARY_DIR NAMES ${GLEW_LIB_NAMES} PATHS ${PROJECT_SOURCE_DIR}/../lib DOC "462 include directories")

  # GLEW include dir
  find_path(GLEW_INCLUDE_DIR NAMES ${GLEW_INC_NAMES} PATHS ${PROJECT_SOURCE_DIR}/../include/GLEW DOC "462 include directories")

  # Version
  set(GLEW_VERSION 1.13.0)

  # Set package standard args
  #include(FindPackageHandleStandardArgs)
  #FIND_PACKAGE_HANDLE_STANDARD_ARGS(GLEW REQUIRED_VARS GLEW_LIBRARY GLEW_INCLUDE_DIR GLEW_LIBRARY_DIR VERSION_VAR GLEW_VERSION)
endmacro(find_glew)
