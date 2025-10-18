set(EXAMPLE_FILES
	example.c
)

add_executable(example ${EXAMPLE_FILES})

set_target_properties(example PROPERTIES
	C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

target_include_directories(example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(example PUBLIC budouxc)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EXAMPLE_FILES})