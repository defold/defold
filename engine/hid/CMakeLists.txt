# Minimum CMake version and project
cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(defold)

project(defold_hid LANGUAGES C CXX)

# Options
#option(BUILD_HID_TESTS "Build HID unit tests" ON)

# ***********************************************************************

# Define log domain for all targets in this directory
add_compile_definitions(DLIB_LOG_DOMAIN="HID")

# Sources
set(HID_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

# hid (native backend)
set(hid_SOURCES )
list(APPEND hid_SOURCES "${HID_SRC_DIR}/hid.cpp")

if (NOT TARGET_PLATFORM MATCHES "x86_64-ps4|x86_64-ps5|arm64-nx64")
  list(APPEND hid_SOURCES "${HID_SRC_DIR}/native/hid_native.cpp")
  list(APPEND hid_SOURCES "${HID_SRC_DIR}/native/hid_gamepad_driver_glfw.cpp")
endif()

add_library(hid STATIC ${hid_SOURCES})

target_include_directories(hid PUBLIC "${HID_SRC_DIR}") # for dmsdk/hid and local headers

# hid_null (null backend)
add_library(hid_null STATIC
  "${HID_SRC_DIR}/hid.cpp"
  "${HID_SRC_DIR}/hid_null.cpp"
)
target_include_directories(hid_null PUBLIC "${HID_SRC_DIR}")


# ===================== Tests =====================

if(BUILD_TESTS)
  # test_hid uses jc_test and links against Defold support libs
  add_executable(test_hid "${HID_SRC_DIR}/test/test_hid.cpp")

  # Do not build test executables by default (optional via explicit target)
  set_target_properties(test_hid PROPERTIES EXCLUDE_FROM_ALL TRUE)

  target_link_libraries(test_hid PRIVATE
    dlib
    profile_null
    platform_null
    hid_null
    testmain
  )

endif()

# Optional interactive test app (creates a tiny window and logs input)
if(BUILD_TESTS)
  set(HID_ADAPTER_SYMBOL GraphicsAdapterOpenGL)
  if(USE_VULKAN)
    set(HID_ADAPTER_SYMBOL GraphicsAdapterVulkan)
  endif()

  # Generate a unit that provides dmExportedSymbols() referencing the adapter symbol
  set(_exported_symbols_cpp "${CMAKE_CURRENT_BINARY_DIR}/hid_exported_symbols.cpp")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake/exported_symbols.in.cpp" "${_exported_symbols_cpp}" @ONLY)

  add_executable(test_app_hid
    "${HID_SRC_DIR}/test/test_app_hid.cpp"
    "${_exported_symbols_cpp}"
  )

  # Optional app should not be part of the default build
  set_target_properties(test_app_hid PROPERTIES EXCLUDE_FROM_ALL TRUE)

  target_link_libraries(test_app_hid PRIVATE
    dlib
    hid
    graphics
    platform
    profile_null
  )

  if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
    target_link_libraries(test_app_hid PRIVATE
      dmglfw EGL GLESv1_CM GLESv2
    )
  else()
    target_link_libraries(test_app_hid PRIVATE
      glfw3
    )
  endif()

  # Platform link hints (adjust if needed)
  if(APPLE)
    target_link_libraries(test_app_hid PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework OpenGL")
  elseif(WIN32)
    target_link_libraries(test_app_hid PRIVATE user32 gdi32 shell32)
  elseif(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(test_app_hid PRIVATE Threads::Threads dl)
  endif()
endif()

if(BUILD_TESTS)

  # Group target to build all executables together
  add_custom_target(build_test
    DEPENDS test_hid test_app_hid
    COMMENT "Building tests"
  )
  set_target_properties(build_test PROPERTIES EXCLUDE_FROM_ALL TRUE)

  # Convenience target to run tests with stdout passthrough
  # This runs the test binaries directly so all output is visible in the console.
  add_custom_target(test
    COMMAND $<TARGET_FILE:test_hid>
    DEPENDS build_test
    USES_TERMINAL
    COMMENT "Running unit tests"
  )
  set_target_properties(test PROPERTIES EXCLUDE_FROM_ALL TRUE)

endif()

# ===================== Install =====================

# Install static libraries to lib/<target-platform>
install(TARGETS hid hid_null
    ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
    LIBRARY DESTINATION lib/${TARGET_PLATFORM}
    RUNTIME DESTINATION bin/${TARGET_PLATFORM}
)

# Install public headers
install(FILES "${HID_SRC_DIR}/hid.h" DESTINATION include/hid)
