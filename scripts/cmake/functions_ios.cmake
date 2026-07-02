if(DEFINED CMAKE_SCRIPT_MODE_FILE AND CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  if(NOT DEFINED DEFOLD_IOS_STAGE_SOURCE)
    message(FATAL_ERROR "functions_ios.cmake stage copy requires DEFOLD_IOS_STAGE_SOURCE")
  endif()

  if(NOT DEFINED DEFOLD_IOS_STAGE_DESTINATION)
    if(NOT DEFINED DEFOLD_IOS_STAGE_BUNDLE_DESTINATION)
      message(FATAL_ERROR "functions_ios.cmake stage copy requires DEFOLD_IOS_STAGE_DESTINATION or DEFOLD_IOS_STAGE_BUNDLE_DESTINATION")
    endif()
    if("$ENV{TARGET_BUILD_DIR}" STREQUAL "" OR "$ENV{WRAPPER_NAME}" STREQUAL "")
      message(FATAL_ERROR "functions_ios.cmake stage copy requires Xcode TARGET_BUILD_DIR and WRAPPER_NAME for DEFOLD_IOS_STAGE_BUNDLE_DESTINATION")
    endif()
    string(REGEX REPLACE "^/+" "" _bundle_destination "${DEFOLD_IOS_STAGE_BUNDLE_DESTINATION}")
    set(DEFOLD_IOS_STAGE_DESTINATION "$ENV{TARGET_BUILD_DIR}/$ENV{WRAPPER_NAME}/${_bundle_destination}")
  endif()

  if(NOT EXISTS "${DEFOLD_IOS_STAGE_SOURCE}")
    message(STATUS "iOS test stage source does not exist, skipping: ${DEFOLD_IOS_STAGE_SOURCE}")
    return()
  endif()

  get_filename_component(_destination_parent "${DEFOLD_IOS_STAGE_DESTINATION}" DIRECTORY)
  file(MAKE_DIRECTORY "${_destination_parent}")

  if(IS_DIRECTORY "${DEFOLD_IOS_STAGE_SOURCE}")
    file(COPY "${DEFOLD_IOS_STAGE_SOURCE}/" DESTINATION "${DEFOLD_IOS_STAGE_DESTINATION}")
  else()
    configure_file("${DEFOLD_IOS_STAGE_SOURCE}" "${DEFOLD_IOS_STAGE_DESTINATION}" COPYONLY)
  endif()

  return()
endif()

defold_log("functions_ios.cmake:")

function(defold_xcode_configure_ios_app target)
  if(NOT (TARGET_PLATFORM STREQUAL "arm64-ios" AND CMAKE_GENERATOR STREQUAL "Xcode"))
    return()
  endif()

  cmake_parse_arguments(DXI "" "BUNDLE_ID;BUNDLE_NAME;VERSION;SHORT_VERSION;BUNDLE_VERSION" "" ${ARGN})

  if(DXI_BUNDLE_ID)
    set(_bundle_id "${DXI_BUNDLE_ID}")
  else()
    set(_bundle_id "com.defold.${target}")
  endif()

  if(DXI_BUNDLE_NAME)
    set(_bundle_name "${DXI_BUNDLE_NAME}")
  else()
    set(_bundle_name "${target}")
  endif()

  if(DXI_SHORT_VERSION)
    set(_short_version "${DXI_SHORT_VERSION}")
  elseif(DXI_VERSION)
    set(_short_version "${DXI_VERSION}")
  else()
    set(_short_version "1")
  endif()

  if(DXI_BUNDLE_VERSION)
    set(_bundle_version "${DXI_BUNDLE_VERSION}")
  elseif(DXI_VERSION)
    set(_bundle_version "${DXI_VERSION}")
  else()
    set(_bundle_version "${_short_version}")
  endif()

  set_target_properties(${target} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "${_bundle_name}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${_bundle_id}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${_short_version}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${_bundle_version}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${_bundle_id}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_INFOPLIST_KEY_LSRequiresIPhoneOS "YES")

  if(DEFOLD_IOS_MOBILEPROVISION)
    set_target_properties(${target} PROPERTIES
      XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
      XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${DEFOLD_IOS_DEVELOPMENT_TEAM}"
      XCODE_ATTRIBUTE_PROVISIONING_PROFILE "${DEFOLD_IOS_PROVISIONING_PROFILE_UUID}"
      XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${DEFOLD_IOS_PROVISIONING_PROFILE_SPECIFIER}"
      XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${DEFOLD_IOS_CODESIGN_IDENTITY}")
  endif()
endfunction()
