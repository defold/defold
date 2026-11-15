include_guard(GLOBAL)

# Build-only helpers stay outside Bob's shipped JAR and project source trees.
function(defold_bob_worker out_classes out_class)
  set(_classes "${CMAKE_BINARY_DIR}/bob-worker")
  set(_class "${_classes}/BobWorker.class")
  if(NOT TARGET defold_bob_worker)
    find_package(Java COMPONENTS Runtime Development REQUIRED)
    set(_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/BobWorker.java")
    add_custom_command(OUTPUT "${_class}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_classes}"
      COMMAND "${Java_JAVAC_EXECUTABLE}" -encoding UTF-8 -cp "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar"
        -d "${_classes}" "${_source}"
      DEPENDS "${_source}" "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar"
      VERBATIM)
    add_custom_target(defold_bob_worker DEPENDS "${_class}")
    if(TARGET bob_light)
      add_dependencies(defold_bob_worker bob_light)
    endif()
  endif()
  set(${out_classes} "${_classes}" PARENT_SCOPE)
  set(${out_class} "${_class}" PARENT_SCOPE)
endfunction()

function(defold_json_string out value)
  string(REPLACE "\\" "\\\\" value "${value}")
  string(REPLACE "\"" "\\\"" value "${value}")
  string(REPLACE "\n" "\\n" value "${value}")
  string(REPLACE "\r" "\\r" value "${value}")
  string(REPLACE "\t" "\\t" value "${value}")
  set(${out} "\"${value}\"" PARENT_SCOPE)
endfunction()

function(defold_json_array out)
  set(_values)
  foreach(_value IN LISTS ARGN)
    defold_json_string(_value "${_value}")
    list(APPEND _values "${_value}")
  endforeach()
  list(JOIN _values "," _json)
  set(${out} "[${_json}]" PARENT_SCOPE)
endfunction()

function(defold_bob_job out folder stamp)
  defold_json_string(_folder "${folder}")
  defold_json_string(_stamp "${stamp}")
  defold_json_array(_dependencies ${ARGN})
  set(${out} "{\"folder\":${_folder},\"stamp\":${_stamp},\"dependencies\":${_dependencies}}" PARENT_SCOPE)
endfunction()
