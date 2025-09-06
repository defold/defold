# Minimum CMake version and project
cmake_minimum_required(VERSION 3.16)
project(defold_hid LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(defold)

# Options
#option(BUILD_HID_TESTS "Build HID unit tests" ON)

# ***********************************************************************

# Define log domain for all targets in this directory
add_compile_definitions(DLIB_LOG_DOMAIN="HID")

# Sources
set(HID_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(HID_NATIVE_DIR "${HID_SRC_DIR}/native")


# hid (native backend)
add_library(hid STATIC
  "${HID_SRC_DIR}/hid.cpp"
  "${HID_NATIVE_DIR}/hid_native.cpp"
  "${HID_NATIVE_DIR}/hid_gamepad_driver_glfw.cpp"
)
target_include_directories(hid
  PUBLIC
    "${HID_SRC_DIR}"            # for dmsdk/hid and local headers
)

# hid_null (null backend)
add_library(hid_null STATIC
  "${HID_SRC_DIR}/hid.cpp"
  "${HID_SRC_DIR}/hid_null.cpp"
)
target_include_directories(hid_null
  PUBLIC
    "${HID_SRC_DIR}"
)

# ***********************************************************************

if(BUILD_TESTS)
  enable_testing()

  # test_hid uses jc_test and links against Defold support libs
  add_executable(test_hid "${HID_SRC_DIR}/test/test_hid.cpp")

  target_link_libraries(test_hid PRIVATE
    hid_null
    testmain
    dlib
    profile_null
    platform_null
  )

  add_test(NAME hid_unit_tests COMMAND test_hid)
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

  target_link_libraries(test_app_hid PRIVATE
    hid
    graphics
    platform
    profile
    dlib
    glfw3
  )


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
