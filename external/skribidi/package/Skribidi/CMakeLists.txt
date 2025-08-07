cmake_minimum_required(VERSION 3.22)
include(FetchContent)
include(CMakeDependentOption)

project(skribidi
	VERSION 0.1.0
	DESCRIPTION "Bidi text rendering and input library"
	HOMEPAGE_URL "https://github.com/memononen/skribidi"
	LANGUAGES C CXX
)

option(ENABLE_ASAN "Enable Address Sanitizer flags" OFF)

if(ENABLE_ASAN)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(STATUS "Enabling Address Sanitizer (-fsanitize=address) for Debug/Clang build")

      set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

      set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
      set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address")

    else()
      message(STATUS "ENABLE_ASAN is ON, but compiler is not Clang (${CMAKE_C_COMPILER_ID}/${CMAKE_CXX_COMPILER_ID}). ASan not enabled.")
    endif()
  else()
    message(STATUS "ENABLE_ASAN is ON, but build type is not Debug (${CMAKE_BUILD_TYPE}). ASan not enabled.")
  endif()
else()
  message(STATUS "ENABLE_ASAN is OFF. ASan not enabled.")
endif()

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

if(MSVC)
	# Make Release mode emit PDB for compiled artifacts
	add_compile_options("$<$<CONFIG:RELEASE>:/Zi>")
	add_link_options("$<$<CONFIG:RELEASE>:/DEBUG>")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_subdirectory(src)

# This hides samples, test, and doxygen from apps that use skribidi via FetchContent
if(PROJECT_IS_TOP_LEVEL)
	option(SKRIBIDI_EXAMPLE "Build the Skribidi example" ON)
	option(SKRIBIDI_UNIT_TESTS "Build the Skribidi unit tests" ON)

	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

	include(GNUInstallDirs)

	install(
		DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include"
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)

	if(SKRIBIDI_UNIT_TESTS OR SKRIBIDI_EXAMPLE)

		# Emscripten pthread support for enkiTS
#		if(EMSCRIPTEN)
#			set(EMSCRIPTEN_PTHREADS_COMPILER_FLAGS "-pthread -s USE_PTHREADS=1")
#			set(EMSCRIPTEN_PTHREADS_LINKER_FLAGS "${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS} -s ALLOW_MEMORY_GROWTH")
#			string(APPEND CMAKE_C_FLAGS " ${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}")
#			string(APPEND CMAKE_CXX_FLAGS " ${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}")
#			string(APPEND CMAKE_EXE_LINKER_FLAGS " ${EMSCRIPTEN_PTHREADS_LINKER_FLAGS}")
#		endif()
	endif()

	if(SKRIBIDI_EXAMPLE)
		add_subdirectory(example)

		# default startup project for Visual Studio
		if(MSVC)
			set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT skribidi_example)
			set_property(TARGET skribidi_example PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
		endif()

		if(APPLE)
			set_target_properties(skribidi_example PROPERTIES
				XCODE_GENERATE_SCHEME TRUE
				XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
		endif()
	endif()

	if(SKRIBIDI_UNIT_TESTS)
		add_subdirectory(test)
		set_property(TARGET skribidi_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
	endif()
endif()
