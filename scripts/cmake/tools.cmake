defold_log("tools.cmake:")

include(tools_clang)
include(tools_java)
include(tools_ninja)
if(NOT DEFINED DEFOLD_REQUIRE_PROTOC OR DEFOLD_REQUIRE_PROTOC)
  include(tools_protoc)
endif()

# Screenshot/report tools are host test dependencies, not engine build requirements.
if(BUILD_TESTS AND TARGET_PLATFORM STREQUAL HOST_PLATFORM)
  include(tools_likeness)
endif()
