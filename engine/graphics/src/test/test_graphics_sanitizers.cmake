function(check_driver_lifetime library mode leaked_bytes)
  execute_process(COMMAND "${PROBE}" "${library}" "${mode}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
  if(leaked_bytes EQUAL 0)
    if(NOT "${result}" STREQUAL "0" OR "${output}${error}" MATCHES "LeakSanitizer")
      message(FATAL_ERROR "Retained driver failed (${result}):\n${output}${error}")
    endif()
  elseif(NOT "${result}" MATCHES "^[1-9][0-9]*$" OR
         NOT "${output}${error}" MATCHES "${leaked_bytes} byte\\(s\\) leaked in 1 allocation\\(s\\)")
    message(FATAL_ERROR "Expected a ${leaked_bytes}-byte leak for ${mode} (${result}):\n${output}${error}")
  endif()
endfunction()

# Reproduce loss of a driver-global root when its module is unloaded.
check_driver_lifetime("${DRIVER}" unload 73)
# Retaining the DRI module preserves its global roots, including repeated calls.
check_driver_lifetime("${DRIVER}" retain 0)
# Application leaks must still fail even when the driver is retained.
check_driver_lifetime("${DRIVER}" leak 139)

get_filename_component(directory "${DRIVER}" DIRECTORY)
set(plugin "${directory}/graphics_test_plugin.so")
file(COPY_FILE "${DRIVER}" "${plugin}")
# Unrelated shared libraries must still unload and report their lost allocations.
check_driver_lifetime("${plugin}" retain 73)
file(REMOVE "${plugin}")
message(STATUS "Graphics sanitizer driver lifetime checks passed")
