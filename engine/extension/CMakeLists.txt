cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_extension LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="EXTENSION")

set(EXTENSION_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(EXTENSION_SRC_DIR "${EXTENSION_ROOT}/src")

set(EXTENSION_SOURCES "${EXTENSION_SRC_DIR}/extension.cpp")
if(TARGET_PLATFORM MATCHES "^(arm64-ios|x86_64-ios)$")
  list(APPEND EXTENSION_SOURCES "${EXTENSION_SRC_DIR}/extension_ios.cpp")
endif()
if(TARGET_PLATFORM MATCHES "^arm.*android$")
  list(APPEND EXTENSION_SOURCES "${EXTENSION_SRC_DIR}/extension_android.cpp")
endif()

defold_add_library(extension STATIC ${EXTENSION_SOURCES})
target_include_directories(extension PUBLIC
  "${EXTENSION_SRC_DIR}"
  "${EXTENSION_ROOT}")
target_compile_definitions(extension PRIVATE "DLIB_LOG_DOMAIN=\"EXTENSION\"")
defold_target_link_libraries(extension "${TARGET_PLATFORM}" SCOPE PUBLIC
  dlib
  image
  mbedtls)

install(TARGETS extension
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES "${EXTENSION_SRC_DIR}/extension.hpp" DESTINATION include/extension)

file(GLOB_RECURSE EXTENSION_CSHARP_SDK_FILES CONFIGURE_DEPENDS
     RELATIVE "${EXTENSION_SRC_DIR}/cs/dmsdk"
     "${EXTENSION_SRC_DIR}/cs/dmsdk/*.cs")
foreach(_extension_cs_file IN LISTS EXTENSION_CSHARP_SDK_FILES)
  get_filename_component(_extension_cs_dir "${_extension_cs_file}" DIRECTORY)
  if(_extension_cs_dir)
    install(FILES "${EXTENSION_SRC_DIR}/cs/dmsdk/${_extension_cs_file}" DESTINATION "sdk/cs/dmsdk/${_extension_cs_dir}")
  else()
    install(FILES "${EXTENSION_SRC_DIR}/cs/dmsdk/${_extension_cs_file}" DESTINATION sdk/cs/dmsdk)
  endif()
endforeach()

if(BUILD_TESTS)
  defold_csharp_nativeaot_runtime_available(_extension_cs_runtime_available "${TARGET_PLATFORM}")
  if(_extension_cs_runtime_available)
    defold_add_csharp_static_library(extension_cs PROJECT "${EXTENSION_SRC_DIR}/cs/extension_cs.csproj")
  endif()
  add_subdirectory(src/test)
endif()
