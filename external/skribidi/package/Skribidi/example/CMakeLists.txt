# glad for OpenGL API
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/extern/glad)

# glfw for windowing and input
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW Docs")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW Examples")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW Tests")
set(GLFW_INSTALL OFF CACHE BOOL "GLFW Install")

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.4
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(glfw)


add_compile_options("$<$<COMPILE_LANG_AND_ID:C,MSVC>:/utf-8>")
add_compile_options("$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>")

set(SKRIBIDI_EXAMPLE_FILES
	debug_draw.c
	debug_draw.h
	example_aligns.c
	example_cached.c
	example_decorations.c
	example_fallback.c
	example_icons.c
	example_inlineobj.c
	example_richtext.c
	example_testbed.c
	ime.c
	ime.h
	main.c
	utils.c
	utils.h
)

add_executable(skribidi_example ${SKRIBIDI_EXAMPLE_FILES})

set_target_properties(skribidi_example PROPERTIES
	C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

target_include_directories(skribidi_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GLAD_DIR}/include)
target_link_libraries(skribidi_example PUBLIC glfw skribidi harfbuzz)

# IME only works on Windows for now.
if(MSVC)
    target_link_libraries(skribidi_example PUBLIC imm32.lib comctl32.lib)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SKRIBIDI_EXAMPLE_FILES})

# Data files
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Copy files on windows, as symlink creation requires special privileges.
    add_custom_command(
        TARGET skribidi_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${CMAKE_SOURCE_DIR}/example/data/
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/)
else()
    # Create symlink on non-windows
    add_custom_command(
        TARGET skribidi_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${CMAKE_SOURCE_DIR}/example/data/
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
    )
endif()
