project(qjs-nanovg C)


add_library(qjs-nanovg SHARED nanovg-qjs.c nanovg-qjs.h)

set_target_properties(qjs-nanovg PROPERTIES OUTPUT_NAME nanovg PREFIX "")

target_include_directories(qjs-nanovg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/src)
target_link_libraries(qjs-nanovg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nanovg/build/libnanovg.a)
target_compile_definitions(qjs-nanovg PRIVATE JS_SHARED_LIBRARY=1)
install(TARGETS qjs-nanovg DESTINATION lib/quickjs)
