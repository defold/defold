defold_log("tools_clang.cmake:")

function(defold_find_clang_cpp OUT_VAR)
    set(_hints)
    set(_vs_roots)

    foreach(_root IN ITEMS "${DEFOLD_VISUAL_STUDIO_ROOT}" "${CMAKE_GENERATOR_INSTANCE}")
        if(_root)
            file(TO_CMAKE_PATH "${_root}" _root)
            list(APPEND _vs_roots "${_root}")
        endif()
    endforeach()

    find_program(_vswhere vswhere HINTS "C:/Program Files (x86)/Microsoft Visual Studio/Installer" NO_CACHE)
    if(_vswhere)
        execute_process(
            COMMAND "${_vswhere}" -all -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
            OUTPUT_VARIABLE _vswhere_roots
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_vswhere_roots)
            string(REPLACE "\n" ";" _vswhere_roots "${_vswhere_roots}")
            list(APPEND _vs_roots ${_vswhere_roots})
        endif()
    endif()

    file(GLOB _local_vs_roots LIST_DIRECTORIES TRUE
        "C:/Program Files/Microsoft Visual Studio/*/*"
        "C:/Program Files (x86)/Microsoft Visual Studio/*/*")
    list(APPEND _vs_roots ${_local_vs_roots})
    list(REMOVE_DUPLICATES _vs_roots)

    foreach(_root IN LISTS _vs_roots)
        foreach(_arch IN ITEMS x64 ARM64 x86)
            list(APPEND _hints "${_root}/VC/Tools/Llvm/${_arch}/bin")
        endforeach()
    endforeach()

    list(APPEND _hints
        "${DEFOLD_SDK_ROOT}/bin/${TARGET_PLATFORM}"
        "${DEFOLD_SDK_ROOT}/bin"
        "${DEFOLD_SDK_ROOT}/ext/bin/${TARGET_PLATFORM}"
        "${DEFOLD_SDK_ROOT}/ext/bin"
        "C:/Program Files/LLVM/bin"
        "C:/Program Files (x86)/LLVM/bin")

    find_program(_clang_cpp NAMES clang++ clang clang-cl HINTS ${_hints} NO_CACHE)
    if(_clang_cpp)
        set(${OUT_VAR} "${_clang_cpp}" CACHE FILEPATH "Clang C++ frontend used to generate Java bindings" FORCE)
    else()
        set(${OUT_VAR} "${OUT_VAR}-NOTFOUND" CACHE FILEPATH "Clang C++ frontend used to generate Java bindings" FORCE)
    endif()
    set(${OUT_VAR} "${${OUT_VAR}}" PARENT_SCOPE)
endfunction()
