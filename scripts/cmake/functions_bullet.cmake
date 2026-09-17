defold_log("functions_bullet.cmake:")

include(functions_libs)

# Link Bullet 3D libraries for a given platform.
#
# Usage:
#   defold_target_link_bullet3d(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>])
#
# This adds the Bullet static libraries used by Defold:
#   - BulletDynamics
#   - BulletCollision
#   - LinearMath
#
function(defold_target_link_bullet3d target platform)
  set(options)
  set(oneValueArgs SCOPE)
  set(multiValueArgs)
  cmake_parse_arguments(DB3D "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT DB3D_SCOPE)
    set(DB3D_SCOPE PRIVATE)
  endif()

  if(NOT target OR NOT platform)
    message(FATAL_ERROR "defold_target_link_bullet3d: target and platform are required")
  endif()

  # Imported targets preserve the filenames produced by build_ext on every
  # platform, including Windows-based consoles.
  foreach(_lib IN ITEMS BulletDynamics BulletCollision LinearMath)
    if(NOT TARGET ${_lib})
      set(_path "${DEFOLD_SDK_ROOT}/ext/lib/${platform}/lib${_lib}${CMAKE_STATIC_LIBRARY_SUFFIX}")
      if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Missing ${_path}. Run ./scripts/build.py --platform=${platform} build_ext first.")
      endif()
      add_library(${_lib} STATIC IMPORTED GLOBAL)
      set_target_properties(${_lib} PROPERTIES IMPORTED_LOCATION "${_path}")
    endif()
  endforeach()

  defold_target_link_libraries(${target} ${platform} SCOPE ${DB3D_SCOPE}
    BulletDynamics BulletCollision LinearMath)
endfunction()
