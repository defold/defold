defold_log("functions_opengl.cmake:")

# Link the appropriate OpenGL system libraries for a given platform tuple.
#
# Usage:
#   include(functions_opengl)
#   defold_target_link_opengl(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>])
#
# Notes:
# - This function links only the GL/GLES system libraries/frameworks. Platform
#   windowing or extra system libs should be added separately.

function(defold_target_link_opengl target platform)
    set(options)
    set(oneValueArgs SCOPE)
    set(multiValueArgs)
    cmake_parse_arguments(DOGL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT DOGL_SCOPE)
        set(DOGL_SCOPE PRIVATE)
    endif()

    if(NOT target OR NOT platform)
        message(FATAL_ERROR "defold_target_link_opengl: target and platform are required")
    endif()

    # Derive OS from tuple (e.g., x86_64-win32 -> win32)
    string(REGEX REPLACE "^[^-]+-" "" _PLAT_OS "${platform}")

    if(_PLAT_OS STREQUAL "win32")
        # Windows OpenGL libraries
        target_link_libraries(${target} ${DOGL_SCOPE} opengl32 glu32)

    elseif(_PLAT_OS STREQUAL "macos")
        # macOS frameworks
        target_link_libraries(${target} ${DOGL_SCOPE}
            "-framework OpenGL" "-framework AGL")

    elseif(_PLAT_OS STREQUAL "ios")
        # iOS uses OpenGLES
        target_link_libraries(${target} ${DOGL_SCOPE} "-framework OpenGLES")

    elseif(_PLAT_OS STREQUAL "linux")
        # Linux OpenGL libraries
        target_link_libraries(${target} ${DOGL_SCOPE} GL GLU)

    elseif(_PLAT_OS STREQUAL "android")
        # Android EGL + GLES
        target_link_libraries(${target} ${DOGL_SCOPE} EGL GLESv1_CM GLESv2)

    else()
        # Web and other platforms: no explicit system GL linkage here
    endif()
endfunction()

