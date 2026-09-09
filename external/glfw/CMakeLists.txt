# The Android ASAN build uses the same sources as lib/wscript. Other builds
# continue to use the packaged GLFW libraries.
file(GLOB GLFW_ANDROID_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/android/*.c")

set(GLFW_GL_SOURCES ${GLFW_ANDROID_SOURCES})
list(FILTER GLFW_GL_SOURCES EXCLUDE REGEX "/android_window_no_api\\.c$")
add_library(dmglfw STATIC ${GLFW_GL_SOURCES})

set(GLFW_VULKAN_SOURCES ${GLFW_ANDROID_SOURCES})
list(FILTER GLFW_VULKAN_SOURCES EXCLUDE REGEX "/(android_util|android_glext|android_window_gl|image)\\.c$")
add_library(dmglfw_vulkan STATIC ${GLFW_VULKAN_SOURCES})
target_compile_definitions(dmglfw_vulkan PRIVATE DMGLFW_NO_GL)

foreach(target dmglfw dmglfw_vulkan)
  target_include_directories(${target} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/android")
  target_link_libraries(${target} PRIVATE defold_sdk)
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endforeach()
