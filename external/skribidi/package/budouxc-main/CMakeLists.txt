cmake_minimum_required(VERSION 3.22)

project(budouxc
	VERSION 0.1.0
	DESCRIPTION "BudouX word breaking implementation in C"
	HOMEPAGE_URL "https://github.com/memononen/budouxc"
	LANGUAGES C
)

# stuff to help debug cmake
# message(STATUS "cmake tool chain: ${CMAKE_TOOLCHAIN_FILE}")
# message(STATUS "cmake source dir: ${CMAKE_SOURCE_DIR}")
# message(STATUS "library postfix: ${CMAKE_DEBUG_POSTFIX}")
message(STATUS "CMake C compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "CMake C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMake system name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMake host system processor: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

# static link
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_subdirectory(src)

# This hides example from apps that use the library via FetchContent
if(PROJECT_IS_TOP_LEVEL)

	message(STATUS "IS TOP LEVEL")

	option(BUDOUXC_EXAMPLE "Build the example" ON)

	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
	set(CMAKE_SUPPRESS_REGENERATION true)

	include(GNUInstallDirs)

	install(
		DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include"
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)

	if(BUDOUXC_EXAMPLE)
		add_subdirectory(example)

		# default startup project for Visual Studio
		if(MSVC)
			set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT example)
			set_property(TARGET example PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
		endif()

		if(APPLE)
			set_target_properties(samples PROPERTIES
				XCODE_GENERATE_SCHEME TRUE
				XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
		endif()
	endif()
endif()
