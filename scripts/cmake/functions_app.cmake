defold_log("functions_app.cmake:")

# Link application-level system frameworks/libs similar to waf's
# FRAMEWORK_APP, STLIB_APP, LIB_APP, LINKFLAGS_APP selections.
#
# Usage:
#   defold_target_link_app(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>])

function(defold_target_link_app target platform)
  set(options)
  set(oneValueArgs SCOPE)
  set(multiValueArgs)
  cmake_parse_arguments(DAPP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT DAPP_SCOPE)
    set(DAPP_SCOPE PRIVATE)
  endif()

  if(NOT target OR NOT platform)
    message(FATAL_ERROR "defold_target_link_app: target and platform are required")
  endif()

  # Derive OS from tuple (e.g., x86_64-win32 -> win32)
  string(REGEX REPLACE "^[^-]+-" "" _PLAT_OS "${platform}")

  if(_PLAT_OS STREQUAL "macos")
    # FRAMEWORK_APP for macOS: pass frameworks as linker options explicitly.
    # Using "-Wl,-framework,<name>" avoids accidental concatenation where
    # only a single "-framework" appears before multiple names.
    foreach(_fw IN ITEMS AppKit Cocoa IOKit Carbon CoreVideo)
      target_link_options(${target} ${DAPP_SCOPE} "-Wl,-framework,${_fw}")
    endforeach()
  elseif(_PLAT_OS STREQUAL "linux")
    # LIB_APP for Linux
    target_link_libraries(${target} ${DAPP_SCOPE} Xext X11 Xi pthread)
  elseif(_PLAT_OS STREQUAL "win32")
    # LINKFLAGS_APP for Windows (plus DINPUT set)
    target_link_libraries(${target} ${DAPP_SCOPE}
      user32.lib shell32.lib dbghelp.lib
      dinput8.lib dxguid.lib xinput9_1_0.lib)
  else()
    # iOS/Android/Web: no additional app libs beyond platform defaults
  endif()
endfunction()
