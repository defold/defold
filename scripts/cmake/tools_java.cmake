message("tools_java.cmake:")

# Check that a usable Java (JRE/JDK) is installed and version >= 21
set(_DEFOLD_REQUIRED_JAVA_VERSION 21)

find_program(_DEFOLD_JAVA_EXECUTABLE NAMES java)
if(NOT _DEFOLD_JAVA_EXECUTABLE)
  message(FATAL_ERROR "Java runtime 'java' not found in PATH. Please install Temurin/OpenJDK 21 or newer and ensure 'java' is on PATH.")
endif()

# java -version prints to stderr on many distributions; capture both
execute_process(
  COMMAND "${_DEFOLD_JAVA_EXECUTABLE}" -version
  OUTPUT_VARIABLE _java_out
  ERROR_VARIABLE _java_err
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE)

set(_java_ver_text "${_java_out}\n${_java_err}")

# Extract the quoted version string after the word version, e.g. version "21.0.1" or "1.8.0_292"
set(_JAVA_VERSION_STRING "")
string(REGEX MATCH "version \"([^\"]+)\"" _m "${_java_ver_text}")
if(CMAKE_MATCH_1)
  set(_JAVA_VERSION_STRING "${CMAKE_MATCH_1}")
else()
  # Fallback: grab the first quoted token
  string(REGEX MATCH "\"([^\"]+)\"" _m2 "${_java_ver_text}")
  if(CMAKE_MATCH_1)
    set(_JAVA_VERSION_STRING "${CMAKE_MATCH_1}")
  endif()
endif()

if(NOT _JAVA_VERSION_STRING)
  message(FATAL_ERROR "Unable to parse Java version from: ${_java_ver_text}")
endif()

# Determine major version: for 9+ it's the first component (e.g. 21.0.1), for 1.x it's the second (e.g. 1.8.0)
string(REGEX MATCH "^[0-9]+" _first_comp "${_JAVA_VERSION_STRING}")
set(_JAVA_VERSION_MAJOR "")
if(_first_comp STREQUAL "1")
  # Split on '.' and take second component
  string(REPLACE "." ";" _parts "${_JAVA_VERSION_STRING}")
  list(LENGTH _parts _parts_len)
  if(_parts_len GREATER 1)
    list(GET _parts 1 _second)
    string(REGEX MATCH "^[0-9]+" _JAVA_VERSION_MAJOR "${_second}")
  endif()
else()
  set(_JAVA_VERSION_MAJOR "${_first_comp}")
endif()

if(NOT _JAVA_VERSION_MAJOR)
  message(FATAL_ERROR "Failed to determine Java major version from: ${_JAVA_VERSION_STRING}")
endif()

# Compare to required version (21)
math(EXPR _JAVA_VERSION_MAJOR_INT "${_JAVA_VERSION_MAJOR}")
if(_JAVA_VERSION_MAJOR_INT LESS ${_DEFOLD_REQUIRED_JAVA_VERSION})
  message(FATAL_ERROR "Java version ${_JAVA_VERSION_STRING} detected (<{_DEFOLD_REQUIRED_JAVA_VERSION}). Please install Java ${_DEFOLD_REQUIRED_JAVA_VERSION} or newer.")
endif()

set(JAVA_VERSION_STRING "${_JAVA_VERSION_STRING}" CACHE STRING "Detected Java version string" FORCE)
set(JAVA_VERSION_MAJOR  "${_JAVA_VERSION_MAJOR_INT}" CACHE STRING "Detected Java major version" FORCE)
message(STATUS "tools_java: java ${JAVA_VERSION_STRING} (major ${JAVA_VERSION_MAJOR}) at ${_DEFOLD_JAVA_EXECUTABLE}")

# Validate javac exists and matches the java version
find_program(_DEFOLD_JAVAC_EXECUTABLE NAMES javac)
if(NOT _DEFOLD_JAVAC_EXECUTABLE)
  message(FATAL_ERROR "Java compiler 'javac' not found in PATH. Please install the JDK (Temurin/OpenJDK ${_DEFOLD_REQUIRED_JAVA_VERSION}+).")
endif()

execute_process(
  COMMAND "${_DEFOLD_JAVAC_EXECUTABLE}" -version
  OUTPUT_VARIABLE _javac_out
  ERROR_VARIABLE _javac_err
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE)

set(_javac_ver_text "${_javac_out}\n${_javac_err}")

# Typical formats:
#  - javac 21.0.1
#  - javac 1.8.0_292
set(_JAVAC_VERSION_STRING "")
string(REGEX MATCH "javac ([0-9][0-9._-]*)" _mjav "${_javac_ver_text}")
if(CMAKE_MATCH_1)
  set(_JAVAC_VERSION_STRING "${CMAKE_MATCH_1}")
else()
  # Fallback: first numeric token
  string(REGEX MATCH "([0-9]+(\.[0-9]+)*)" _mver "${_javac_ver_text}")
  if(CMAKE_MATCH_1)
    set(_JAVAC_VERSION_STRING "${CMAKE_MATCH_1}")
  endif()
endif()

if(NOT _JAVAC_VERSION_STRING)
  message(FATAL_ERROR "Unable to parse javac version from: ${_javac_ver_text}")
endif()

string(REGEX MATCH "^[0-9]+" _javac_first "${_JAVAC_VERSION_STRING}")
set(_JAVAC_VERSION_MAJOR "")
if(_javac_first STREQUAL "1")
  string(REPLACE "." ";" _parts "${_JAVAC_VERSION_STRING}")
  list(LENGTH _parts _plen)
  if(_plen GREATER 1)
    list(GET _parts 1 _second)
    string(REGEX MATCH "^[0-9]+" _JAVAC_VERSION_MAJOR "${_second}")
  endif()
else()
  set(_JAVAC_VERSION_MAJOR "${_javac_first}")
endif()

if(NOT _JAVAC_VERSION_MAJOR)
  message(FATAL_ERROR "Failed to determine javac major version from: ${_JAVAC_VERSION_STRING}")
endif()

math(EXPR _JAVAC_VERSION_MAJOR_INT "${_JAVAC_VERSION_MAJOR}")
if(_JAVAC_VERSION_MAJOR_INT LESS 21)
  message(FATAL_ERROR "javac version ${_JAVAC_VERSION_STRING} detected (<21). Please install JDK 21 or newer.")
endif()

if(NOT _JAVAC_VERSION_STRING STREQUAL _JAVA_VERSION_STRING)
  message(FATAL_ERROR "Java/javac version mismatch: java=${_JAVA_VERSION_STRING}, javac=${_JAVAC_VERSION_STRING}. Please ensure both are the same JDK.")
endif()

set(JAVAC_VERSION_STRING "${_JAVAC_VERSION_STRING}" CACHE STRING "Detected javac version string" FORCE)
set(JAVAC_VERSION_MAJOR  "${_JAVAC_VERSION_MAJOR_INT}" CACHE STRING "Detected javac major version" FORCE)
message(STATUS "tools_java: javac ${JAVAC_VERSION_STRING} (major ${JAVAC_VERSION_MAJOR}) at ${_DEFOLD_JAVAC_EXECUTABLE}")
