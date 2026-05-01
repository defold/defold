set(BUDOUXC_SOURCE_FILES
	budoux.c
	model_ja.h
	model_th.h
	model_zh_hans.h
	model_zh_hant.h
)

set(BUDOUXC_API_FILES
	../include/budoux.h
)

add_library(budouxc ${BUDOUXC_SOURCE_FILES} ${BUDOUXC_API_FILES})

target_include_directories(budouxc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_DEBUG_POSTFIX "d")

# budouxc uses C17
set_target_properties(budouxc PROPERTIES
	C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS YES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
	DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "src" FILES ${BUDOUXC_SOURCE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../include" PREFIX "include" FILES ${BUDOUXC_API_FILES})

add_library(budouxc::budouxc ALIAS budouxc)

install(
  TARGETS budouxc
  EXPORT budouxcConfig
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  EXPORT budouxcConfig
  NAMESPACE budouxc::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/budouxc"
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/budouxcConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/budouxcConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/budouxc"
)