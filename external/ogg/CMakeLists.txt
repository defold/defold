cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
set(DEFOLD_REQUIRE_PROTOC OFF)
include(defold)

project(ogg_external VERSION 1.1.1 LANGUAGES C)

if(NOT TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-linux|x86_64-linux|x86_64-win32)$")
  message(FATAL_ERROR "Unsupported Ogg tools platform: ${TARGET_PLATFORM}")
endif()

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)
set(OGG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/package/libogg-1.3.5")
set(OGGZ_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/package/liboggz-1.1.1")
set(OGG_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# All supported tool platforms provide these C99 types and headers.
set(INCLUDE_INTTYPES_H 1)
set(INCLUDE_STDINT_H 1)
set(INCLUDE_SYS_TYPES_H 1)
set(SIZE16 int16_t)
set(USIZE16 uint16_t)
set(SIZE32 int32_t)
set(USIZE32 uint32_t)
set(SIZE64 int64_t)
set(USIZE64 uint64_t)
configure_file("${OGG_SOURCE_DIR}/include/ogg/config_types.h.in"
  "${OGG_GENERATED_DIR}/ogg/config_types.h" @ONLY)

# Windows uses the typedef in oggz_off_t.h. Unix desktop off_t is 64 bits.
set(TYPEOF_OGGZ_OFF_T off_t)
if(APPLE)
  set(PRI_OGGZ_OFF_T ll)
else()
  set(PRI_OGGZ_OFF_T l)
endif()
configure_file("${OGGZ_SOURCE_DIR}/include/oggz/oggz_off_t_generated.h.in"
  "${OGG_GENERATED_DIR}/oggz/oggz_off_t_generated.h" @ONLY)
configure_file(config.h.in "${OGG_GENERATED_DIR}/config.h" @ONLY)

add_library(ogg SHARED "${OGG_SOURCE_DIR}/src/bitwise.c" "${OGG_SOURCE_DIR}/src/framing.c")
target_link_libraries(ogg PRIVATE defold_sdk)
target_include_directories(ogg PUBLIC "${OGG_SOURCE_DIR}/include" "${OGG_GENERATED_DIR}")

set(OGGZ_SOURCES
  dirac.c metric_internal.c oggz.c oggz_auto.c oggz_comments.c oggz_dlist.c
  oggz_io.c oggz_read.c oggz_seek.c oggz_stream.c oggz_table.c oggz_vector.c oggz_write.c)
list(TRANSFORM OGGZ_SOURCES PREPEND "${OGGZ_SOURCE_DIR}/src/liboggz/")
add_library(oggz SHARED ${OGGZ_SOURCES})
target_link_libraries(oggz PRIVATE ogg defold_sdk)
target_include_directories(oggz PUBLIC "${OGGZ_SOURCE_DIR}/include" "${OGG_GENERATED_DIR}")

add_executable(oggz-validate "${OGGZ_SOURCE_DIR}/src/tools/oggz-validate.c"
  "${OGGZ_SOURCE_DIR}/src/tools/oggz_tools.c" "${OGGZ_SOURCE_DIR}/src/liboggz/dirac.c")
target_link_libraries(oggz-validate PRIVATE oggz ogg defold_sdk)
target_include_directories(oggz-validate PRIVATE "${OGGZ_SOURCE_DIR}/src/liboggz")

# Keep the unversioned lib-prefixed names used by Bob and ResourceUnpacker,
# including on Windows. Use upstream's export lists there.
set_target_properties(ogg oggz PROPERTIES PREFIX "lib")
if(WIN32)
  # Upstream's LIBRARY ogg makes consumers import ogg.dll even when /OUT names
  # the file libogg.dll. Keep the source intact and match our packaged DLL name.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OGG_SOURCE_DIR}/win32/ogg.def")
  file(READ "${OGG_SOURCE_DIR}/win32/ogg.def" OGG_EXPORTS)
  string(REPLACE "LIBRARY ogg" "LIBRARY libogg.dll" OGG_EXPORTS "${OGG_EXPORTS}")
  file(CONFIGURE OUTPUT "${OGG_GENERATED_DIR}/ogg.def" CONTENT "${OGG_EXPORTS}" @ONLY)
  target_sources(ogg PRIVATE "${OGG_GENERATED_DIR}/ogg.def")
  target_sources(oggz PRIVATE "${OGGZ_SOURCE_DIR}/win32/liboggz.def")
  target_sources(oggz-validate PRIVATE "${OGGZ_SOURCE_DIR}/win32/attgetopt.c")
  target_include_directories(oggz-validate PRIVATE "${OGGZ_SOURCE_DIR}/win32")
  if(CMAKE_HOST_WIN32)
    # Catch missing or misnamed runtime DLL imports before packaging the tools.
    add_custom_command(TARGET oggz-validate POST_BUILD
      COMMAND "$<TARGET_FILE:oggz-validate>" -v
      COMMENT "Checking Ogg DLL dependencies"
      VERBATIM)
  endif()
else()
  # Override the SDK's hidden visibility after its inherited compile options.
  set_source_files_properties("${OGG_SOURCE_DIR}/src/bitwise.c" "${OGG_SOURCE_DIR}/src/framing.c"
    ${OGGZ_SOURCES} PROPERTIES COMPILE_OPTIONS -fvisibility=default)
  if(APPLE)
    # Bob and the Editor must be able to load/run these tools on macOS 11.
    set_target_properties(ogg oggz oggz-validate PROPERTIES DEFOLD_MACOS_DEPLOYMENT_TARGET 11.0)
    set(OGG_ORIGIN "@loader_path")
    set_target_properties(ogg oggz PROPERTIES MACOSX_RPATH ON INSTALL_NAME_DIR "@rpath")
  else()
    set(OGG_ORIGIN "$ORIGIN")
  endif()
  set_target_properties(ogg oggz oggz-validate PROPERTIES
    SKIP_BUILD_RPATH OFF BUILD_WITH_INSTALL_RPATH ON
    INSTALL_RPATH "${OGG_ORIGIN};${OGG_ORIGIN}/../../lib/${TARGET_PLATFORM}")
endif()

install(TARGETS ogg oggz
  RUNTIME DESTINATION "lib/${TARGET_PLATFORM}"
  LIBRARY DESTINATION "lib/${TARGET_PLATFORM}"
  ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(TARGETS oggz-validate RUNTIME DESTINATION "bin/${TARGET_PLATFORM}")
if(WIN32)
  # The installed validator also needs its DLLs beside the executable. Bob
  # packages the copies from ext/lib together with it in libexec/<platform>.
  install(FILES "$<TARGET_FILE:ogg>" "$<TARGET_FILE:oggz>" DESTINATION "bin/${TARGET_PLATFORM}")
endif()
install(FILES "${OGG_SOURCE_DIR}/COPYING" DESTINATION share/licenses/libogg)
install(FILES "${OGGZ_SOURCE_DIR}/COPYING" DESTINATION share/licenses/liboggz)
