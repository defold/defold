cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(harfbuzz_external VERSION 13.2.1 LANGUAGES C CXX)

set(HARFBUZZ_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package/harfbuzz-13.2.1/src")

# Keep default-axis style metadata for Skribidi without the variable-font
# interpreter. Patch only the build copy; the packaged upstream source stays
# intact. Check the anchor so an upstream update cannot silently drop this.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${HARFBUZZ_SOURCE_DIR}/hb-style.cc" "${HARFBUZZ_SOURCE_DIR}/harfbuzz.cc")
file(READ "${HARFBUZZ_SOURCE_DIR}/hb-style.cc" HARFBUZZ_STYLE)
set(STYLE_ANCHOR "#endif\n\n  if (style_tag == HB_STYLE_TAG_OPTICAL_SIZE")
string(FIND "${HARFBUZZ_STYLE}" "${STYLE_ANCHOR}" STYLE_ANCHOR_POSITION)
if(STYLE_ANCHOR_POSITION EQUAL -1)
  message(FATAL_ERROR "Review the default-instance style patch for this HarfBuzz version")
endif()
set(STYLE_DEFAULTS [=[#else
  hb_blob_ptr_t<OT::fvar> fvar (hb_sanitize_context_t ().reference_table<OT::fvar> (face));
  hb_ot_var_axis_info_t axis;
  bool found = fvar->find_axis_info (style_tag, &axis);
  fvar.destroy ();
  if (found)
    return axis.default_value;
#endif

  if (style_tag == HB_STYLE_TAG_OPTICAL_SIZE]=])
string(REPLACE "${STYLE_ANCHOR}" "${STYLE_DEFAULTS}" HARFBUZZ_STYLE "${HARFBUZZ_STYLE}")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hb-style-defaults.cc" CONTENT "${HARFBUZZ_STYLE}" @ONLY)
file(READ "${HARFBUZZ_SOURCE_DIR}/harfbuzz.cc" HARFBUZZ_AMALGAMATION)
string(FIND "${HARFBUZZ_AMALGAMATION}" "#include \"hb-style.cc\"" STYLE_INCLUDE_POSITION)
if(STYLE_INCLUDE_POSITION EQUAL -1)
  message(FATAL_ERROR "Review the style include in this HarfBuzz amalgamation")
endif()
string(REPLACE "#include \"hb-style.cc\"" "#include \"hb-style-defaults.cc\"" HARFBUZZ_AMALGAMATION "${HARFBUZZ_AMALGAMATION}")
set(HARFBUZZ_AMALGAMATION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/harfbuzz.cc")
file(CONFIGURE OUTPUT "${HARFBUZZ_AMALGAMATION_SOURCE}" CONTENT "${HARFBUZZ_AMALGAMATION}" @ONLY)

add_library(harfbuzz STATIC "${HARFBUZZ_AMALGAMATION_SOURCE}")
target_include_directories(harfbuzz PUBLIC "${HARFBUZZ_SOURCE_DIR}")
target_compile_definitions(harfbuzz PRIVATE
  NDEBUG HB_TINY HB_EXTERN=
  "HB_CONFIG_OVERRIDE_H=\"${CMAKE_CURRENT_LIST_DIR}/config-override.h\"")
# Source options come after the shared toolchain's optimization flags.
set_source_files_properties("${HARFBUZZ_AMALGAMATION_SOURCE}"
  PROPERTIES COMPILE_OPTIONS "$<IF:$<BOOL:${MSVC}>,/O2,-O3>")
target_link_libraries(harfbuzz PRIVATE defold_sdk)
set_target_properties(harfbuzz PROPERTIES PREFIX "lib")

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS harfbuzz ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
file(GLOB HARFBUZZ_HEADERS CONFIGURE_DEPENDS "${HARFBUZZ_SOURCE_DIR}/*.h" "${HARFBUZZ_SOURCE_DIR}/*.hh")
list(FILTER HARFBUZZ_HEADERS EXCLUDE REGEX "/config-override\\.h$")
install(FILES ${HARFBUZZ_HEADERS} DESTINATION include/harfbuzz)
install(FILES config-override.h DESTINATION include/harfbuzz)
