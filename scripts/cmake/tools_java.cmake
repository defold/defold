message("tools_java.cmake:")

# Require Java 21+ (both Runtime and Development components)
find_package(Java 21 COMPONENTS Runtime Development REQUIRED)

# Cache paths and version for visibility and reuse
set(DEFOLD_JAVA_EXECUTABLE  "${Java_JAVA_EXECUTABLE}"  CACHE FILEPATH "Java runtime executable (java)" FORCE)
set(DEFOLD_JAVAC_EXECUTABLE "${Java_JAVAC_EXECUTABLE}" CACHE FILEPATH "Java compiler executable (javac)" FORCE)
set(JAVA_VERSION_STRING     "${Java_VERSION}"          CACHE STRING   "Detected Java version string" FORCE)

# Derive major for convenience (and maintain old cache var name)
string(REGEX MATCH "^[0-9]+" JAVA_VERSION_MAJOR "${Java_VERSION}")
set(JAVA_VERSION_MAJOR "${JAVA_VERSION_MAJOR}" CACHE STRING "Detected Java major version" FORCE)

message(STATUS "tools_java: java ${Java_VERSION} at ${Java_JAVA_EXECUTABLE}")
message(STATUS "tools_java: javac at ${Java_JAVAC_EXECUTABLE}")
