defold_log("functions_codesigning.cmake:")

function(defold_codesign_target target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "defold_codesign_target: target not found: ${target}")
    endif()

    if(DEFOLD_SKIP_CODESIGN)
        return()
    endif()
    if(NOT TARGET_PLATFORM MATCHES "macos|win32$")
        return()
    endif()

    if(DEFOLD_PYTHON)
        set(_defold_codesign_python "${DEFOLD_PYTHON}")
    else()
        find_program(_defold_codesign_python NAMES python3 python REQUIRED)
    endif()

    set(_defold_codesign_args
        --platform "${TARGET_PLATFORM}"
        --file "$<TARGET_FILE:${target}>")
    if(DEFOLD_CODESIGNING_IDENTITY)
        list(APPEND _defold_codesign_args --codesigning-identity "${DEFOLD_CODESIGNING_IDENTITY}")
    endif()
    if(DEFOLD_GCLOUD_PROJECTID)
        list(APPEND _defold_codesign_args --gcloud-projectid "${DEFOLD_GCLOUD_PROJECTID}")
    endif()
    if(DEFOLD_GCLOUD_LOCATION)
        list(APPEND _defold_codesign_args --gcloud-location "${DEFOLD_GCLOUD_LOCATION}")
    endif()
    if(DEFOLD_GCLOUD_KEYRINGNAME)
        list(APPEND _defold_codesign_args --gcloud-keyringname "${DEFOLD_GCLOUD_KEYRINGNAME}")
    endif()
    if(DEFOLD_GCLOUD_KEYNAME)
        list(APPEND _defold_codesign_args --gcloud-keyname "${DEFOLD_GCLOUD_KEYNAME}")
    endif()
    if(DEFOLD_GCLOUD_CERTFILE)
        list(APPEND _defold_codesign_args --gcloud-certfile "${DEFOLD_GCLOUD_CERTFILE}")
    endif()
    if(DEFOLD_GCLOUD_KEYFILE)
        list(APPEND _defold_codesign_args --gcloud-keyfile "${DEFOLD_GCLOUD_KEYFILE}")
    endif()

    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${_defold_codesign_python}" "${DEFOLD_HOME}/build_tools/codesigning.py" ${_defold_codesign_args}
        COMMENT "Signing ${target}"
        VERBATIM)
endfunction()
