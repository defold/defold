cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_jni LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="JNI")

set(JNI_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(JNI_SRC_DIR "${JNI_ROOT}/src")

set(JNI_IS_DESKTOP OFF)
if(TARGET_PLATFORM MATCHES "macos|linux|win32$")
  set(JNI_IS_DESKTOP ON)
endif()

if(JNI_IS_DESKTOP)
  find_package(JNI REQUIRED)

  file(GLOB JNI_SOURCES CONFIGURE_DEPENDS "${JNI_SRC_DIR}/jni/*.cpp")

  defold_add_library(jni STATIC ${JNI_SOURCES})
  target_include_directories(jni PUBLIC "${JNI_SRC_DIR}" ${JNI_INCLUDE_DIRS})

  defold_add_library(jni_noasan STATIC ${JNI_SOURCES})
  target_include_directories(jni_noasan PUBLIC "${JNI_SRC_DIR}" ${JNI_INCLUDE_DIRS})
  target_compile_definitions(jni_noasan PUBLIC VALGRIND)

  install(TARGETS jni jni_noasan
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION lib/${TARGET_PLATFORM})

  install(FILES "${JNI_ROOT}/scripts/gen_java.py" DESTINATION lib/python)
  install(FILES
    "${JNI_ROOT}/scripts/external/gen_ir.py"
    "${JNI_ROOT}/scripts/external/gen_util.py"
    DESTINATION lib/python/external)
  install(FILES "${JNI_SRC_DIR}/jni/jni_util.h" DESTINATION include/jni)

  if(BUILD_TESTS)
    add_subdirectory(src/test)
  endif()
elseif(BUILD_TESTS)
  add_custom_target(build_tests)
endif()
