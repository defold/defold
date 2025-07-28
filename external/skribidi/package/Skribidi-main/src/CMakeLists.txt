include(FetchContent)

if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW) # avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24+
endif()

# harfbuzz
FetchContent_Declare(
	harfbuzz
	GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
	GIT_TAG 11.0.0
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
set(HB_BUILD_SUBSET OFF)
FetchContent_MakeAvailable(harfbuzz)

# sheenbidi
FetchContent_Declare(
	SheenBidi
	GIT_REPOSITORY https://github.com/Tehreer/SheenBidi.git
	GIT_TAG 83f77108a2873600283f6da4b326a2dca7a3a7a6 # Jun 6, 2025
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SheenBidi)

# libunibreak
FetchContent_Declare(
	libunibreak
	URL https://github.com/adah1972/libunibreak/archive/refs/tags/libunibreak_6_1.zip
)
FetchContent_MakeAvailable(libunibreak)

# build manually
add_library(libunibreak STATIC)
if(MSVC)
    target_compile_options(libunibreak PRIVATE /MP)
endif()
target_sources(libunibreak PRIVATE
	${libunibreak_SOURCE_DIR}/src/eastasianwidthdata.c
	${libunibreak_SOURCE_DIR}/src/eastasianwidthdef.c
	${libunibreak_SOURCE_DIR}/src/eastasianwidthdef.h
	${libunibreak_SOURCE_DIR}/src/emojidata.c
	${libunibreak_SOURCE_DIR}/src/emojidef.c
	${libunibreak_SOURCE_DIR}/src/emojidef.h
	${libunibreak_SOURCE_DIR}/src/graphemebreak.c
	${libunibreak_SOURCE_DIR}/src/graphemebreak.h
	${libunibreak_SOURCE_DIR}/src/graphemebreakdata.c
	${libunibreak_SOURCE_DIR}/src/graphemebreakdef.h
	${libunibreak_SOURCE_DIR}/src/indicconjunctbreakdata.c
	${libunibreak_SOURCE_DIR}/src/indicconjunctbreakdef.h
	${libunibreak_SOURCE_DIR}/src/linebreak.c
	${libunibreak_SOURCE_DIR}/src/linebreak.h
	${libunibreak_SOURCE_DIR}/src/linebreakdata.c
	${libunibreak_SOURCE_DIR}/src/linebreakdef.c
	${libunibreak_SOURCE_DIR}/src/linebreakdef.h
	${libunibreak_SOURCE_DIR}/src/unibreakbase.c
	${libunibreak_SOURCE_DIR}/src/unibreakbase.h
	${libunibreak_SOURCE_DIR}/src/unibreakdef.c
	${libunibreak_SOURCE_DIR}/src/unibreakdef.h
	${libunibreak_SOURCE_DIR}/src/wordbreak.c
	${libunibreak_SOURCE_DIR}/src/wordbreak.h
	${libunibreak_SOURCE_DIR}/src/wordbreakdata.c
	${libunibreak_SOURCE_DIR}/src/wordbreakdef.h
)

target_include_directories(libunibreak PUBLIC $<BUILD_INTERFACE:${libunibreak_SOURCE_DIR}/src>)

install(
  TARGETS libunibreak
  EXPORT libunibreakConfig
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  EXPORT libunibreakConfig
  NAMESPACE libunibreak::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libunibreak"
)

# budouxc
FetchContent_Declare(
	budouxc
	GIT_REPOSITORY https://github.com/memononen/budouxc.git
	GIT_TAG a044d49afc654117fac7623fff15bec15943270c
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(budouxc)

#
# Project files
#
set(SKRIBIDI_SOURCE_FILES
	emoji_data.h
	skb_canvas.c
	skb_common.c
	skb_common_internal.h
	skb_editor.c
	skb_font_collection.c
	skb_font_collection_internal.h
	skb_icon_collection.c
	skb_icon_collection_internal.h
	skb_image_atlas.c
	skb_layout.c
	skb_layout_cache.c
	skb_rasterizer.c
)

set(SKRIBIDI_API_FILES
	../include/skb_canvas.h
	../include/skb_common.h
	../include/skb_editor.h
	../include/skb_font_collection.h
	../include/skb_icon_collection.h
	../include/skb_image_atlas.h
	../include/skb_layout.h
	../include/skb_layout_cache.h
	../include/skb_rasterizer.h
)


add_library(skribidi ${SKRIBIDI_SOURCE_FILES} ${SKRIBIDI_API_FILES})

target_include_directories(skribidi
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_DEBUG_POSTFIX "d")

set_target_properties(skribidi PROPERTIES
	C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS YES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
	DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

if(NOT MSVC)
	target_link_libraries(skribidi PUBLIC m)
endif()

target_link_libraries(skribidi PRIVATE harfbuzz SheenBidi libunibreak budouxc)

if(MINGW)
    target_compile_options(harfbuzz PRIVATE -Wa,-mbig-obj)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "src" FILES ${SKRIBIDI_SOURCE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../include" PREFIX "include" FILES ${SKRIBIDI_API_FILES})

install(
  TARGETS skribidi
  EXPORT skribidiConfig
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  EXPORT skribidiConfig
  NAMESPACE skribidi::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/skribidi"
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/skribidiConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/skribidiConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/skribidi"
)
