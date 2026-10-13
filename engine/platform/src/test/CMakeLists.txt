cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

project(defold_platform_tests LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# test_platform (unit tests using null backend)
defold_add_executable(test_platform
    "${CMAKE_CURRENT_LIST_DIR}/test_platform.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_native_touch.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../native/native_touch.c")
target_compile_definitions(test_platform PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")

# Include the platform src directory for headers
target_include_directories(test_platform PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")

defold_target_link_libraries(test_platform "${TARGET_PLATFORM}"
    dlib
    profile_null
    testmain
    platform_null
)

defold_target_link_socket(test_platform "${TARGET_PLATFORM}" PRIVATE)

add_dependencies(test_platform platform_null)

# Register into global build_tests and run_tests with working directory set to engine/input
defold_register_test_target(test_platform ON "${CMAKE_CURRENT_LIST_DIR}/../..")

if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
    defold_add_executable(test_native_web "${CMAKE_CURRENT_LIST_DIR}/test_native_web.cpp")
    defold_target_link_libraries(test_native_web "${TARGET_PLATFORM}"
        dlib
        profile_null
        testmain
        platform)
    defold_register_test_target(test_native_web ON "${CMAKE_CURRENT_LIST_DIR}/../..")

    _defold_find_wasm_runner(_platform_js_test_runner)
    if(_platform_js_test_runner)
        add_custom_target(run_test_library_platform_js
            COMMAND "${_platform_js_test_runner}" "${CMAKE_CURRENT_LIST_DIR}/test_library_platform.js"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
            USES_TERMINAL
            VERBATIM)
        if(TARGET run_tests)
            add_dependencies(run_tests run_test_library_platform_js)
        endif()
    else()
        message(WARNING "Bun or Node.js not found; run_test_library_platform_js is unavailable")
    endif()
endif()

if(TARGET_PLATFORM STREQUAL "wasm-web")
    # This target runs in a browser and must not use the Node filesystem prelude.
    defold_add_executable(test_native_web_browser "${CMAKE_CURRENT_LIST_DIR}/test_native_web_browser.cpp")
    defold_target_link_libraries(test_native_web_browser "${TARGET_PLATFORM}"
        dlib
        profile_null
        platform)
    target_link_options(test_native_web_browser PRIVATE -sENVIRONMENT=web -sASSERTIONS=1 -sEXIT_RUNTIME=0)
    defold_register_test_target(test_native_web_browser OFF)

    find_program(_platform_browser_node NAMES node nodejs)
    if(NOT _platform_browser_node)
        set(_platform_browser_node node)
    endif()
    add_custom_target(run_test_native_web_browser
        COMMAND "${CMAKE_COMMAND}" -E env
            "DEFOLD_BROWSER_TEST_JS=$<TARGET_FILE:test_native_web_browser>"
            "${_platform_browser_node}" "${CMAKE_CURRENT_LIST_DIR}/browser/node_modules/@playwright/test/cli.js" test
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/browser"
        DEPENDS test_native_web_browser
        USES_TERMINAL
        VERBATIM)
endif()

if(TARGET_PLATFORM MATCHES "^(arm64-ios|arm64_sim-ios)$")
    defold_add_executable(test_platform_safe_area
        "${CMAKE_CURRENT_LIST_DIR}/test_platform_safe_area.mm")
    defold_target_link_libraries(test_platform_safe_area "${TARGET_PLATFORM}"
        dlib profile_null testmain platform)
    defold_target_link_app(test_platform_safe_area "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_opengl(test_platform_safe_area "${TARGET_PLATFORM}" PRIVATE)
    target_link_libraries(test_platform_safe_area PRIVATE "-framework QuartzCore")
    defold_target_link_socket(test_platform_safe_area "${TARGET_PLATFORM}" PRIVATE)
    defold_register_test_target(test_platform_safe_area ON "${CMAKE_CURRENT_LIST_DIR}/../..")
endif()

if(TARGET_PLATFORM MATCHES "-android$")
    foreach(backend IN ITEMS gl no_api)
        set(test_target "test_platform_android_insets_${backend}")
        defold_add_executable(${test_target}
            "${CMAKE_CURRENT_LIST_DIR}/test_platform_android_insets.cpp")
        target_include_directories(${test_target} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/../native"
            "${CMAKE_CURRENT_LIST_DIR}/../native/android")
        defold_target_link_libraries(${test_target} "${TARGET_PLATFORM}" dlib profile_null testmain)
        if(backend STREQUAL "gl")
            target_link_libraries(${test_target} PRIVATE platform)
        else()
            target_compile_definitions(${test_target} PRIVATE DM_PLATFORM_NO_OPENGL)
            target_link_libraries(${test_target} PRIVATE platform_vulkan)
        endif()
        target_link_options(${test_target} PRIVATE
            "-Wl,--wrap=dmNativeAcquireAndroidWindow"
            "-Wl,--wrap=dmNativeReleaseAndroidWindow"
            "-Wl,--wrap=ANativeWindow_getWidth"
            "-Wl,--wrap=ANativeWindow_getHeight")
        defold_target_link_socket(${test_target} "${TARGET_PLATFORM}" PRIVATE)
        defold_register_test_target(${test_target} ON "${CMAKE_CURRENT_LIST_DIR}/../..")
    endforeach()
endif()

# ******************************************************************
# test_platform_app (windowed test application)

if (NOT (TARGET_PLATFORM MATCHES "wasm-web|wasm_pthread-web|x86_64-xbone"))

    defold_add_executable(test_platform_app
        "${CMAKE_CURRENT_LIST_DIR}/test_platform_app.cpp")
    target_compile_definitions(test_platform_app PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")

    # Include the platform src directory for headers
    target_include_directories(test_platform_app PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")

    # Platform specific extras
    if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android|x86_64-android")
    elseif (TARGET_PLATFORM MATCHES "arm64-ios|arm64_sim-ios")
        target_link_libraries(test_platform_app PRIVATE "-framework IOKit" "-framework QuartzCore")
    elseif (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
        target_link_libraries(test_platform_app PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    elseif (TARGET_PLATFORM MATCHES "x86_64-win32")
        target_link_libraries(test_platform_app PRIVATE user32 gdi32 shell32)
    elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        find_package(Threads REQUIRED)
        target_link_libraries(test_platform_app PRIVATE Threads::Threads dl)
    endif()

    defold_target_link_libraries(test_platform_app "${TARGET_PLATFORM}"
        dlib
        profile_null
    )

    defold_target_link_opengl(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_glfw(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_platform(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_socket(test_platform_app "${TARGET_PLATFORM}" PRIVATE)

    add_dependencies(test_platform_app platform)
    if(TARGET platform_vulkan)
        add_dependencies(test_platform_app platform_vulkan)
    endif()

    defold_register_test_target(test_platform_app FALSE)
endif()
