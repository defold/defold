cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

project(defold_platform_tests LANGUAGES ${DEFOLD_LANGUAGE_LIST})

if(NOT TARGET_PLATFORM MATCHES "wasm-web|wasm_pthread-web")
    # The fake window backend keeps service tests independent of engine and graphics.
    defold_add_executable(test_platform_service "${CMAKE_CURRENT_LIST_DIR}/test_platform_service.cpp")
    target_include_directories(test_platform_service PRIVATE "${DEFOLD_HOME}/engine/dlib/src")
    defold_target_link_libraries(test_platform_service "${TARGET_PLATFORM}" platform_service dlib profile_null testmain)
    defold_target_link_socket(test_platform_service "${TARGET_PLATFORM}" PRIVATE)
    defold_register_test_target(test_platform_service ON "${CMAKE_CURRENT_LIST_DIR}/../.."
        RUN_GROUP platform_service)
endif()

# test_platform (unit tests using null backend)
defold_add_executable(test_platform
    "${CMAKE_CURRENT_LIST_DIR}/test_platform.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_mobile_input.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../native/input.c"
    "${CMAKE_CURRENT_LIST_DIR}/test_native_touch.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../native/native_touch.c")
target_compile_definitions(test_platform PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")

target_include_directories(test_platform PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/.."
    "${CMAKE_CURRENT_LIST_DIR}/../native")

defold_target_link_libraries(test_platform "${TARGET_PLATFORM}"
    dlib
    profile_null
    testmain
    platform_null
)

defold_target_link_socket(test_platform "${TARGET_PLATFORM}" PRIVATE)

add_dependencies(test_platform platform_null)

# Exercise the vendor fallbacks with a backend that supplies no density or client-size queries.
defold_add_executable(test_platform_vendor
    "${CMAKE_CURRENT_LIST_DIR}/test_platform.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../window.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../platform_window_null.cpp")
target_compile_definitions(test_platform_vendor PRIVATE DM_PLATFORM_VENDOR)
defold_target_link_libraries(test_platform_vendor "${TARGET_PLATFORM}" dlib profile_null testmain)
defold_target_link_socket(test_platform_vendor "${TARGET_PLATFORM}" PRIVATE)
defold_register_test_target(test_platform_vendor ON "${CMAKE_CURRENT_LIST_DIR}/../..")

if(PLATFORM_USE_LINUX OR PLATFORM_USE_COCOA OR PLATFORM_USE_WIN32)
    target_sources(test_platform PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/test_window_state.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../window_state.cpp")

endif()

if(PLATFORM_USE_LINUX OR PLATFORM_USE_WIN32)
    # Exercise the native Windows placement code with deterministic DPI transitions.
    defold_add_executable(test_window_placement_dpi
        "${CMAKE_CURRENT_LIST_DIR}/test_window_placement_dpi.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../win32/window_placement.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../window_placement.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../window_state.cpp")
    target_compile_definitions(test_window_placement_dpi PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
    defold_target_link_libraries(test_window_placement_dpi "${TARGET_PLATFORM}" dlib profile_null testmain)
    defold_target_link_socket(test_window_placement_dpi "${TARGET_PLATFORM}" PRIVATE)
    defold_register_test_target(test_window_placement_dpi ON "${CMAKE_CURRENT_LIST_DIR}/../..")

endif()

if(PLATFORM_USE_LINUX OR PLATFORM_USE_COCOA OR PLATFORM_USE_WIN32)
    defold_add_executable(test_window_placement "${CMAKE_CURRENT_LIST_DIR}/test_window_placement.cpp")
    defold_target_link_libraries(test_window_placement "${TARGET_PLATFORM}" dlib profile_null testmain platform)
    defold_target_link_opengl(test_window_placement "${TARGET_PLATFORM}" PRIVATE)

    defold_target_link_socket(test_window_placement "${TARGET_PLATFORM}" PRIVATE)
    if(TARGET_PLATFORM_OS STREQUAL "macos")
        target_sources(test_window_placement PRIVATE test_window_placement_cocoa.mm test_window_cocoa.mm)
        target_compile_options(test_window_placement PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fno-rtti>")
        target_link_libraries(test_window_placement PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework CoreGraphics")
    elseif(PLATFORM_USE_LINUX)
        target_sources(test_window_placement PRIVATE test_window_placement_linux.cpp)
    elseif(TARGET_PLATFORM_OS STREQUAL "win32")
        target_sources(test_window_placement PRIVATE test_window_placement_win32.cpp test_window_opengl_win32.cpp)
        target_link_libraries(test_window_placement PRIVATE user32 gdi32 shell32)
    endif()
    # Requires a desktop session; the geometry/codec tests above also run headless.
    defold_register_test_target(test_window_placement FALSE)
endif()

if(PLATFORM_USE_LINUX)
    defold_add_executable(test_window_linux test_window_linux.cpp)
    defold_target_link_libraries(test_window_linux "${TARGET_PLATFORM}" platform_vulkan dlib profile_null)
    defold_register_test_target(test_window_linux FALSE)

    defold_add_executable(test_linux_joystick test_linux_joystick.cpp)
    defold_target_link_libraries(test_linux_joystick "${TARGET_PLATFORM}" platform dlib profile_null)
    foreach(syscall IN ITEMS open ioctl scandir inotify_add_watch)
        target_link_options(test_linux_joystick PRIVATE "-Wl,--wrap=${syscall}")
    endforeach()
    defold_register_test_target(test_linux_joystick ON)

    # Graphics is added after platform in a full engine build.
    function(add_linux_window_test_runner)
        _defold_find_python(_python)
        set(_targets test_platform test_window_placement test_window_linux test_linux_joystick)
        if(TARGET test_graphics_window_resize_vulkan)
            list(APPEND _targets test_graphics_window_resize_vulkan)
            set(_vulkan_args --vulkan $<TARGET_FILE:test_graphics_window_resize_vulkan>)
        endif()
        add_custom_target(run_linux_window_tests
            COMMAND $<TARGET_FILE:test_linux_joystick>
            COMMAND "${_python}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/run_linux_window_tests.py"
                --placement $<TARGET_FILE:test_window_placement> --native $<TARGET_FILE:test_window_linux>
                --headless $<TARGET_FILE:test_platform> ${_vulkan_args}
            DEPENDS ${_targets}
            USES_TERMINAL VERBATIM)
    endfunction()
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL add_linux_window_test_runner)
endif()

if(PLATFORM_USE_COCOA)
    defold_add_executable(test_window_joystick_cocoa test_window_joystick_cocoa.mm)
    defold_target_link_libraries(test_window_joystick_cocoa "${TARGET_PLATFORM}" platform dlib profile_null)
    defold_register_test_target(test_window_joystick_cocoa FALSE)

    if(CMAKE_HOST_APPLE)
        execute_process(COMMAND xcrun --find ibtool OUTPUT_VARIABLE _ibtool
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(EXISTS "${_ibtool}")
            # Unbundled test executables also search their own directory for MainMenu.nib.
            set(_test_menu_dir "${CMAKE_CURRENT_BINARY_DIR}/menu_fixture")
            set(_test_menu_nib "${_test_menu_dir}/MainMenu.nib")
            add_custom_command(OUTPUT "${_test_menu_nib}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${_test_menu_dir}"
                COMMAND "${_ibtool}" --compile "${_test_menu_nib}" "${CMAKE_CURRENT_SOURCE_DIR}/test_window_menu.xib"
                DEPENDS test_window_menu.xib VERBATIM)
            set_source_files_properties("${_test_menu_nib}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
            defold_add_executable(test_window_menu_cocoa test_window_menu_cocoa.mm "${_test_menu_nib}")
            set_target_properties(test_window_menu_cocoa PROPERTIES MACOSX_BUNDLE TRUE)
            defold_target_link_libraries(test_window_menu_cocoa "${TARGET_PLATFORM}" platform dlib profile_null)
            defold_register_test_target(test_window_menu_cocoa FALSE)
        endif()
    endif()
endif()

# Register into global build_tests and run_tests with working directory set to engine/input
defold_register_test_target(test_platform ON "${CMAKE_CURRENT_LIST_DIR}/../..")

if(TARGET_PLATFORM STREQUAL "x86_64-win32")
    defold_add_executable(test_win32_joystick test_win32_joystick.cpp)
    defold_target_link_libraries(test_win32_joystick "${TARGET_PLATFORM}" platform dlib profile_null testmain)
    defold_target_link_socket(test_win32_joystick "${TARGET_PLATFORM}")
    defold_register_test_target(test_win32_joystick ON "${CMAKE_CURRENT_LIST_DIR}/../..")

    defold_add_executable(test_platform_win32
        "${CMAKE_CURRENT_LIST_DIR}/test_platform_win32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../../engine/src/engine.rc")
    target_include_directories(test_platform_win32 PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../../engine/src")
    defold_target_link_libraries(test_platform_win32 "${TARGET_PLATFORM}"
        dlib profile_null testmain platform)
    target_link_libraries(test_platform_win32 PRIVATE user32 gdi32 shell32)
    defold_target_link_opengl(test_platform_win32 "${TARGET_PLATFORM}")
    defold_target_link_socket(test_platform_win32 "${TARGET_PLATFORM}")
    defold_register_test_target(test_platform_win32 ON "${CMAKE_CURRENT_LIST_DIR}/../..")
endif()

if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
    defold_add_executable(test_native_web "${CMAKE_CURRENT_LIST_DIR}/test_native_web.cpp")
    defold_target_link_libraries(test_native_web "${TARGET_PLATFORM}"
        dlib
        profile_null
        testmain
        platform)
    defold_register_test_target(test_native_web ON "${CMAKE_CURRENT_LIST_DIR}/../..")
endif()

# The browser library and loader tests use a host JS runtime and need no WASM build.
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
    defold_add_executable(test_platform_home_indicator
        "${CMAKE_CURRENT_LIST_DIR}/test_platform_home_indicator.mm")
    defold_target_link_libraries(test_platform_home_indicator "${TARGET_PLATFORM}"
        dlib profile_null testmain platform)
    defold_target_link_app(test_platform_home_indicator "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_opengl(test_platform_home_indicator "${TARGET_PLATFORM}" PRIVATE)
    target_link_libraries(test_platform_home_indicator PRIVATE "-framework QuartzCore")
    defold_target_link_socket(test_platform_home_indicator "${TARGET_PLATFORM}" PRIVATE)
    defold_register_test_target(test_platform_home_indicator ON "${CMAKE_CURRENT_LIST_DIR}/../..")

    defold_add_executable(test_platform_frame_rate
        "${CMAKE_CURRENT_LIST_DIR}/test_platform_frame_rate.mm")
    defold_target_link_libraries(test_platform_frame_rate "${TARGET_PLATFORM}"
        dlib profile_null testmain platform)
    defold_target_link_app(test_platform_frame_rate "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_opengl(test_platform_frame_rate "${TARGET_PLATFORM}" PRIVATE)
    target_link_libraries(test_platform_frame_rate PRIVATE "-framework QuartzCore")
    defold_target_link_socket(test_platform_frame_rate "${TARGET_PLATFORM}" PRIVATE)
    defold_register_test_target(test_platform_frame_rate ON "${CMAKE_CURRENT_LIST_DIR}/../..")

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
    defold_add_executable(test_platform_android_joystick
        "${CMAKE_CURRENT_LIST_DIR}/test_platform_android_joystick.cpp")
    target_include_directories(test_platform_android_joystick PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../native"
        "${CMAKE_CURRENT_LIST_DIR}/../native/android")
    defold_target_link_libraries(test_platform_android_joystick "${TARGET_PLATFORM}" dlib profile_null testmain platform)
    foreach(function IN ITEMS AInputEvent_getDeviceId AInputEvent_getType AInputEvent_getSource
            AKeyEvent_getKeyCode AKeyEvent_getAction AKeyEvent_getFlags AKeyEvent_getMetaState
            AKeyEvent_getScanCode AKeyEvent_getRepeatCount AKeyEvent_getDownTime AKeyEvent_getEventTime
            AMotionEvent_getAction AMotionEvent_getAxisValue)
        target_link_options(test_platform_android_joystick PRIVATE "-Wl,--wrap=${function}")
    endforeach()
    defold_target_link_socket(test_platform_android_joystick "${TARGET_PLATFORM}" PRIVATE)
    defold_register_test_target(test_platform_android_joystick ON "${CMAKE_CURRENT_LIST_DIR}/../..")

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

    defold_target_link_platform(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_socket(test_platform_app "${TARGET_PLATFORM}" PRIVATE)

    add_dependencies(test_platform_app platform)
    if(TARGET platform_vulkan)
        add_dependencies(test_platform_app platform_vulkan)
    endif()

    defold_register_test_target(test_platform_app FALSE)
endif()
