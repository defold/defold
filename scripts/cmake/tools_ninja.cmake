defold_log("tools_ninja.cmake:")

# Check that Ninja is installed and available on PATH

find_program(_DEFOLD_NINJA_EXECUTABLE NAMES ninja ninja-build)
if(NOT _DEFOLD_NINJA_EXECUTABLE)
  message(FATAL_ERROR "Ninja build tool not found. Please install Ninja and ensure 'ninja' is on PATH.")
endif()

# Try to print detected version (optional)
execute_process(
  COMMAND "${_DEFOLD_NINJA_EXECUTABLE}" --version
  OUTPUT_VARIABLE _ninja_ver
  ERROR_VARIABLE _ninja_err
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE)

if(_ninja_ver)
  defold_log("tools_ninja: ninja ${_ninja_ver} at ${_DEFOLD_NINJA_EXECUTABLE}")
else()
  defold_log("tools_ninja: ninja found at ${_DEFOLD_NINJA_EXECUTABLE}")
endif()
