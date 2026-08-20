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

  # Use the Defold lib linker helper to handle Windows name prefixing
  defold_target_link_libraries(${target} ${platform} SCOPE ${DB3D_SCOPE}
    BulletDynamics BulletCollision LinearMath)
endfunction()
