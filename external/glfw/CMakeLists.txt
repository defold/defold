cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_glfw VERSION 2.7.1 LANGUAGES ${DEFOLD_LANGUAGE_LIST})

if(TARGET_PLATFORM MATCHES "^(arm64-ios|arm64_sim-ios)$")
  set(GLFW_PLATFORM_DIR ios)
elseif(TARGET_PLATFORM MATCHES "-macos$")
  set(GLFW_PLATFORM_DIR cocoa)
elseif(TARGET_PLATFORM MATCHES "-android$")
  set(GLFW_PLATFORM_DIR android)
elseif(TARGET_PLATFORM MATCHES "-linux$")
  set(GLFW_PLATFORM_DIR x11)
elseif(TARGET_PLATFORM MATCHES "-win32$")
  set(GLFW_PLATFORM_DIR win32)
elseif(TARGET_PLATFORM MATCHES "-web$")
  set(GLFW_PLATFORM_DIR web)
else()
  message(FATAL_ERROR "GLFW is not supported on ${TARGET_PLATFORM}")
endif()

if(GLFW_PLATFORM_DIR STREQUAL "web")
  file(GLOB GLFW_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/lib/web/*.c")
else()
  file(GLOB GLFW_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/lib/*.c")
  if(GLFW_PLATFORM_DIR STREQUAL "ios")
    file(GLOB GLFW_PLATFORM_SOURCES CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_LIST_DIR}/lib/ios/*.m"
      "${CMAKE_CURRENT_LIST_DIR}/lib/ios/app/*.m")
  elseif(GLFW_PLATFORM_DIR STREQUAL "cocoa")
    file(GLOB GLFW_PLATFORM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/lib/cocoa/*.m")
  else()
    file(GLOB GLFW_PLATFORM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/lib/${GLFW_PLATFORM_DIR}/*.c")
  endif()
  list(APPEND GLFW_SOURCES ${GLFW_PLATFORM_SOURCES})
endif()

set(GLFW_TARGETS dmglfw)
if(GLFW_PLATFORM_DIR STREQUAL "android")
  set(GLFW_VULKAN_SOURCES ${GLFW_SOURCES})
  list(FILTER GLFW_VULKAN_SOURCES EXCLUDE REGEX "/(android_util|android_glext|android_window_gl|image)\\.c$")
  list(FILTER GLFW_SOURCES EXCLUDE REGEX "/android_window_no_api\\.c$")
  add_library(dmglfw_vulkan STATIC ${GLFW_VULKAN_SOURCES})
  target_compile_definitions(dmglfw_vulkan PRIVATE DMGLFW_NO_GL)
  list(APPEND GLFW_TARGETS dmglfw_vulkan)
endif()

add_library(dmglfw STATIC ${GLFW_SOURCES})
foreach(_target IN LISTS GLFW_TARGETS)
  target_include_directories(${_target} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/lib"
    "${CMAKE_CURRENT_LIST_DIR}/lib/${GLFW_PLATFORM_DIR}")
  target_compile_definitions(${_target} PRIVATE SDL_JOYSTICK_IOKIT)
  target_link_libraries(${_target} PRIVATE defold_sdk)
  set_target_properties(${_target} PROPERTIES PREFIX "lib")
endforeach()

if(GLFW_PLATFORM_DIR STREQUAL "x11")
  find_package(X11 REQUIRED)
  target_include_directories(dmglfw PRIVATE ${X11_INCLUDE_DIR})
  target_compile_definitions(dmglfw PRIVATE
    GL_GLEXT_PROTOTYPES _GLFW_HAS_GLXGETPROCADDRESS _GLFW_USE_LINUX_JOYSTICKS
    _GLFW_HAS_PTHREAD _GLFW_HAS_SCHED_YIELD _GLFW_HAS_SYSCONF)
endif()

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

if(GLFW_PLATFORM_DIR STREQUAL "android")
  set(GLFW_ANDROID_CLASSES "${CMAKE_CURRENT_BINARY_DIR}/java")
  set(GLFW_ANDROID_JAR "${CMAKE_CURRENT_BINARY_DIR}/glfw_android.jar")
  file(GLOB_RECURSE GLFW_ANDROID_JAVA CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/java/*.java")
  add_custom_command(
    OUTPUT "${GLFW_ANDROID_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${GLFW_ANDROID_CLASSES}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GLFW_ANDROID_CLASSES}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
      -classpath "${DEFOLD_ANDROID_JAR}" -d "${GLFW_ANDROID_CLASSES}" ${GLFW_ANDROID_JAVA}
    COMMAND "${Java_JAR_EXECUTABLE}" cf "${GLFW_ANDROID_JAR}" -C "${GLFW_ANDROID_CLASSES}" .
    DEPENDS ${GLFW_ANDROID_JAVA} "${DEFOLD_ANDROID_JAR}"
    VERBATIM)
  add_custom_target(glfw_android_jar ALL DEPENDS "${GLFW_ANDROID_JAR}")
  install(FILES "${GLFW_ANDROID_JAR}" DESTINATION share/java)
elseif(GLFW_PLATFORM_DIR STREQUAL "web")
  install(FILES js/library_glfw.js DESTINATION "lib/${TARGET_PLATFORM}/js")
endif()

install(TARGETS ${GLFW_TARGETS} ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(FILES include/GL/glfw.h include/GL/glfw_native.h DESTINATION include/glfw)
