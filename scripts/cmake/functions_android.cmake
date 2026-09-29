defold_log("functions_android.cmake:")

# Check the linked artifact, including focused builds outside scripts/build.py.
function(defold_validate_android_elf target)
  if(NOT TARGET_PLATFORM MATCHES "^(arm64|x86_64)-android$")
    return()
  endif()
  get_target_property(_type ${target} TYPE)
  if(NOT _type MATCHES "^(EXECUTABLE|SHARED_LIBRARY|MODULE_LIBRARY)$")
    return()
  endif()
  if(NOT CMAKE_READELF)
    message(FATAL_ERROR "Android ELF validation requires the NDK llvm-readelf (CMAKE_READELF)")
  endif()
  if(DEFOLD_PYTHON)
    set(_python "${DEFOLD_PYTHON}")
  elseif(Python3_EXECUTABLE)
    set(_python "${Python3_EXECUTABLE}")
  else()
    find_program(_python NAMES python3 python REQUIRED)
  endif()

  set(_validator "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../check_android_elf.py")
  # Recheck existing outputs when the validation rules change.
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${_validator}")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${_python}" "${_validator}" --readelf "${CMAKE_READELF}" "$<TARGET_FILE:${target}>"
    COMMENT "Checking 16 KB ELF alignment for ${target}"
    VERBATIM)
endfunction()
