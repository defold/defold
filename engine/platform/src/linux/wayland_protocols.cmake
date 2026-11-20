find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_get_variable(_wayland_core wayland-scanner pkgdatadir)
pkg_get_variable(_wayland_protocols wayland-protocols pkgdatadir)
set(_wayland_generated "${CMAKE_CURRENT_BINARY_DIR}/wayland")
file(MAKE_DIRECTORY "${_wayland_generated}")
set(_wayland_xml
    "${_wayland_core}/wayland.xml"
    "${_wayland_protocols}/stable/xdg-shell/xdg-shell.xml"
    "${_wayland_protocols}/stable/viewporter/viewporter.xml"
    "${_wayland_protocols}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
    "${_wayland_protocols}/unstable/relative-pointer/relative-pointer-unstable-v1.xml"
    "${_wayland_protocols}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
    "${_wayland_protocols}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml"
    "${_wayland_protocols}/staging/xdg-activation/xdg-activation-v1.xml"
    "${CMAKE_CURRENT_LIST_DIR}/fractional-scale-v1.xml")
foreach(_xml IN LISTS _wayland_xml)
    get_filename_component(_name "${_xml}" NAME_WE)
    set(_code "${_wayland_generated}/${_name}-protocol.c")
    set(_header "${_wayland_generated}/${_name}-client-protocol.h")
    add_custom_command(OUTPUT "${_code}" "${_header}"
        COMMAND "${WAYLAND_SCANNER}" private-code "${_xml}" "${_code}"
        COMMAND "${WAYLAND_SCANNER}" client-header "${_xml}" "${_header}"
        DEPENDS "${_xml}" VERBATIM)
    list(APPEND _wayland_protocol_sources "${_code}" "${_header}")
endforeach()
# Build the protocol metadata once. Both window libraries use these private symbols.
add_library(platform_wayland_protocols OBJECT ${_wayland_protocol_sources})
set_target_properties(platform_wayland_protocols PROPERTIES POSITION_INDEPENDENT_CODE ON)
