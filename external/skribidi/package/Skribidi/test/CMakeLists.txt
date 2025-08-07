# unit test app

set(SKRIBIDI_TEST_FILES
	test_basic.c
	test_canvas.c
	test_cpp.cpp
	test_editor.c
	test_font_collection.c
	test_hashtable.c
	test_icon_collection.c
	test_layout.c
	test_layout_cache.c
	test_rasterizer.c
	test_image_atlas.c
	test_tempalloc.c
    test_macros.h
    tester.c
)

add_executable(skribidi_test ${SKRIBIDI_TEST_FILES})

set_target_properties(skribidi_test PROPERTIES
    C_STANDARD 17
    CXX_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

# Special access to internals for testing
target_include_directories(skribidi_test PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(skribidi_test PRIVATE skribidi)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "" FILES ${SKRIBIDI_TEST_FILES})

# Data files
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Copy files on windows, as symlink creation requires special privileges.
    add_custom_command(
        TARGET skribidi_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${CMAKE_SOURCE_DIR}/example/data/
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/)
else()
    # Create symlink on non-windows
    add_custom_command(
        TARGET skribidi_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${CMAKE_SOURCE_DIR}/example/data/
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
    )
endif()
