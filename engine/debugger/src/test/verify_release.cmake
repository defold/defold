# Validate the compiled artifact, including the extension registration entry.
file(STRINGS "${ARCHIVE}" DEBUGGER_STRINGS REGEX "LuaDebugger|dmDebugger|Content-Length:|supportsConfigurationDoneRequest")
if(DEBUGGER_STRINGS)
  message(FATAL_ERROR "Debugger implementation leaked into a DM_RELEASE artifact: ${DEBUGGER_STRINGS}")
endif()
message(STATUS "DM_RELEASE archive contains no debugger implementation or registration")
