defold_log("tools_java.cmake:")

# Require Java 25+ (both Runtime and Development components)
find_package(Java 25 COMPONENTS Runtime Development REQUIRED)

# Cache paths and version for visibility and reuse
set(DEFOLD_JAVA_EXECUTABLE  "${Java_JAVA_EXECUTABLE}"  CACHE FILEPATH "Java runtime executable (java)" FORCE)
set(DEFOLD_JAVAC_EXECUTABLE "${Java_JAVAC_EXECUTABLE}" CACHE FILEPATH "Java compiler executable (javac)" FORCE)
set(JAVA_VERSION_STRING     "${Java_VERSION}"          CACHE STRING   "Detected Java version string" FORCE)

set(_DEFOLD_JAVA_RUNTIME_FLAGS "$ENV{DM_JAVA_RUNTIME_FLAGS}")
if(NOT _DEFOLD_JAVA_RUNTIME_FLAGS)
  set(_DEFOLD_JAVA_RUNTIME_FLAGS "--sun-misc-unsafe-memory-access=allow --enable-native-access=ALL-UNNAMED")
endif()
set(DEFOLD_JAVA_RUNTIME_FLAGS "${_DEFOLD_JAVA_RUNTIME_FLAGS}" CACHE STRING "Java runtime flags for Defold build tools" FORCE)
separate_arguments(DEFOLD_JAVA_RUNTIME_FLAGS_LIST NATIVE_COMMAND "${DEFOLD_JAVA_RUNTIME_FLAGS}")
unset(_DEFOLD_JAVA_RUNTIME_FLAGS)

# Derive major for convenience (and maintain old cache var name)
string(REGEX MATCH "^[0-9]+" JAVA_VERSION_MAJOR "${Java_VERSION}")
set(JAVA_VERSION_MAJOR "${JAVA_VERSION_MAJOR}" CACHE STRING "Detected Java major version" FORCE)

defold_log("tools_java: java ${Java_VERSION} at ${Java_JAVA_EXECUTABLE}")
defold_log("tools_java: javac at ${Java_JAVAC_EXECUTABLE}")
