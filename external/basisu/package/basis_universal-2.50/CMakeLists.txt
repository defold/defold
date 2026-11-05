# Important: The Basis Universal encoder and transcoder libraries must be compiled with -fno-strict-aliasing (MSVC's default, and also the Linux kernel).
# It should also work without this option, but we do not test with it.
cmake_minimum_required(VERSION 3.20)

project(basisu C CXX)

# pybind11: allow old Python finder modules without complaining
if (POLICY CMP0148)
    cmake_policy(SET CMP0148 OLD)
endif()

add_compile_definitions($<$<CONFIG:Debug>:_DEBUG=1>)

if (CMAKE_SYSTEM_NAME STREQUAL "WASI")
    set(BASISU_BUILD_WASM TRUE)
else()
    set(BASISU_BUILD_WASM FALSE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BASISU_STATIC "static linking" FALSE)
option(BASISU_SAN "sanitize" FALSE)
option(BASISU_EXAMPLES "build examples" TRUE)
option(BASISU_WASM_THREADING "Enable WASI threading support" OFF)
option(BASISU_BUILD_PYTHON "Build native Python module via pybind11" OFF)
option(BASISU_DISABLE_ANDROID_ASTC_DECOMP "Disable Android ASTC decompressor" OFF)
option(BASISU_SUPPORT_ASTCENC "Enable astcenc support in encoder" FALSE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# For MSVC builds default to SSE enabled, and determine if it's a 64-bit (-A x64) vs. 32-bit (-A Win32) build.
if (MSVC)
    option(BASISU_SSE "SSE 4.1 support" TRUE)
    if ( CMAKE_GENERATOR_PLATFORM STREQUAL Win32 )    
        set(BASISU_BUILD_X64 0)
    else()
        set(BASISU_BUILD_X64 1)
    endif()
    add_compile_options(/W4)
else()
    option(BASISU_SSE "SSE 4.1 support" FALSE)
    option(BASISU_BUILD_X64 "build 64-bit" TRUE)
endif()

option(BASISU_ZSTD "ZSTD support for KTX2 transcoding/encoding" TRUE)
option(BASISU_OPENCL "OpenCL support in encoder" FALSE)

# Old option to new (BASISU_ prefixed) automatic remapping
foreach(pair
    "STATIC;BASISU_STATIC"
    "SAN;BASISU_SAN"
    "EXAMPLES;BASISU_EXAMPLES"
    "WASM_THREADING;BASISU_WASM_THREADING"
    "BUILD_PYTHON;BASISU_BUILD_PYTHON"
    "BUILD_X64;BASISU_BUILD_X64"
    "SSE;BASISU_SSE"
    "ZSTD;BASISU_ZSTD"
    "OPENCL;BASISU_OPENCL"
)
    list(GET pair 0 OLD)
    list(GET pair 1 NEW)

    if(DEFINED ${OLD})
        message(WARNING "[BASISU] Legacy option '${OLD}' is deprecated. Use '${NEW}' instead.")
        set(${NEW} "${${OLD}}" CACHE BOOL "" FORCE)
    endif()
endforeach()

if (BASISU_BUILD_WASM)
    message(STATUS "Configuring for WASM (WASI-SDK)")

    # WASM is always 32-bit
    set(BASISU_BUILD_X64 OFF CACHE BOOL "" FORCE)

    # WASM cannot use SSE
    set(BASISU_SSE OFF CACHE BOOL "" FORCE)

    # WASM cannot use OpenCL
    set(BASISU_OPENCL OFF CACHE BOOL "" FORCE)

    # WASM cannot use static linking
    set(BASISU_STATIC OFF CACHE BOOL "" FORCE)

    # WASM cannot use sanitizers
    set(BASISU_SAN OFF CACHE BOOL "" FORCE)
endif()

message("Initial CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
message("Initial BASISU_BUILD_X64=${BASISU_BUILD_X64}")
message("Initial BASISU_BUILD_WASM=${BASISU_BUILD_WASM}")
message("Initial BASISU_WASM_THREADING=${BASISU_WASM_THREADING}")
message("Initial BASISU_BUILD_PYTHON=${BASISU_BUILD_PYTHON}")
message("Initial BASISU_SSE=${BASISU_SSE}")
message("Initial BASISU_ZSTD=${BASISU_ZSTD}")
message("Initial BASISU_OPENCL=${BASISU_OPENCL}")
message("Initial BASISU_SAN=${BASISU_SAN}")
message("Initial BASISU_EXAMPLES=${BASISU_EXAMPLES}")
message("Initial BASISU_DISABLE_ANDROID_ASTC_DECOMP=${BASISU_DISABLE_ANDROID_ASTC_DECOMP}")
message("Initial BASISU_SUPPORT_ASTCENC=${BASISU_SUPPORT_ASTCENC}")

if ((NOT MSVC) AND BASISU_OPENCL)
    # With MSVC builds we use the Khronos lib/include files in the project's "OpenCL" directory, to completely avoid requiring fiddly to install vendor SDK's.
    # Otherwise we use the system's (if any).
    find_package(OpenCL)
    message(STATUS "OpenCL found: ${OPENCL_FOUND}")
    message(STATUS "OpenCL includes: ${OpenCL_INCLUDE_DIRS}")
    message(STATUS "OpenCL libraries: ${OpenCL_LIBRARIES}")
endif()

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release )
endif()

message(${PROJECT_NAME} " build type: " ${CMAKE_BUILD_TYPE})

if (BASISU_BUILD_X64)
    message("Building 64-bit")
else()
    message("Building 32-bit")
endif()

if (BASISU_SSE)
    message("SSE enabled")
else()
    message("SSE disabled")
endif()

if (BASISU_ZSTD)
    message("Zstandard enabled")
else()
    message("Zstandard disabled")
endif()

if (BASISU_DISABLE_ANDROID_ASTC_DECOMP)
    add_compile_definitions(BASISU_DISABLE_ANDROID_ASTC_DECOMP=1)
else()
    add_compile_definitions(BASISU_DISABLE_ANDROID_ASTC_DECOMP=0)
endif()

if (NOT MSVC AND NOT BASISU_BUILD_WASM)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
   
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

    if (BASISU_SAN)
        message("Enabling SAN")
        
        set(SANITIZE_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize=alignment")
        
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${SANITIZE_FLAGS}")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${SANITIZE_FLAGS}")
    
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${SANITIZE_FLAGS}")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${SANITIZE_FLAGS}")
    endif()

    # set(CMAKE_CXX_FLAGS -std=c++17)
    set(GCC_COMPILE_FLAGS "-fvisibility=hidden -fPIC -fno-strict-aliasing -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-misleading-indentation")
   
    if (NOT BASISU_BUILD_X64)
        set(GCC_COMPILE_FLAGS "${GCC_COMPILE_FLAGS} -m32")
    endif()

    if (EMSCRIPTEN)
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -DBASISU_SUPPORT_SSE=0")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -DBASISU_SUPPORT_SSE=0")

        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_LINK_FLAGS}")
    elseif (BASISU_STATIC)
        if (BASISU_SSE)
            set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DBASISU_SUPPORT_SSE=1 -msse4.1")
            set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBASISU_SUPPORT_SSE=1 -msse4.1")
        else()
            set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DBASISU_SUPPORT_SSE=0")
            set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBASISU_SUPPORT_SSE=0")
        endif()
      
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_LINK_FLAGS} -static-libgcc -static-libstdc++ -static")
    else()
        if (BASISU_SSE)
            set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DBASISU_SUPPORT_SSE=1 -msse4.1")
            set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBASISU_SUPPORT_SSE=1 -msse4.1")
        else()
            set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DBASISU_SUPPORT_SSE=0")
            set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBASISU_SUPPORT_SSE=0")
        endif()
      
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_LINK_FLAGS} -Wl,-rpath .")
    endif()

    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${GCC_COMPILE_FLAGS}")
    set(CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE} ${GCC_COMPILE_FLAGS}")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${GCC_COMPILE_FLAGS} -D_DEBUG")

    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COMPILE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} ${GCC_COMPILE_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${GCC_COMPILE_FLAGS} -D_DEBUG")
elseif (BASISU_BUILD_WASM)
    # _WASI_EMULATED_PROCESS_CLOCKS/-lwasi-emulated-process-clocks is only for ZStd
    add_compile_definitions(_WASI_EMULATED_PROCESS_CLOCKS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lwasi-emulated-process-clocks")

    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fno-strict-aliasing -fvisibility=hidden -Wall -Wextra -Wno-unknown-warning-option")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -fvisibility=hidden -Wall -Wextra -Wno-unknown-warning-option")

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
    
    # We need a few MB of stack - don't skip this or WASMTime will silently allow the stack to grow into the heap or static memory, causing corruption.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--stack-first -Wl,-z,stack-size=8388608")
else()
    if (BASISU_SSE)
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DBASISU_SUPPORT_SSE=1")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBASISU_SUPPORT_SSE=1")
    else()
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DBASISU_SUPPORT_SSE=0")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBASISU_SUPPORT_SSE=0")
    endif()
endif()

# Define the source files for the static library
set(ENCODER_LIB_SRC_LIST
    encoder/basisu_backend.cpp
    encoder/basisu_basis_file.cpp
    encoder/basisu_comp.cpp
    encoder/basisu_enc.cpp
    encoder/basisu_etc.cpp
    encoder/basisu_frontend.cpp
    encoder/basisu_gpu_texture.cpp
    encoder/basisu_pvrtc1_4.cpp
    encoder/basisu_resampler.cpp
    encoder/basisu_resample_filters.cpp
    encoder/basisu_ssim.cpp
    encoder/basisu_uastc_enc.cpp
    encoder/basisu_bc7e_scalar.cpp
    encoder/basisu_dds_export.cpp
    encoder/basisu_bc7enc.cpp
    encoder/jpgd.cpp
    encoder/basisu_kernels_sse.cpp
    encoder/basisu_bc15_spmd.cpp
    encoder/basisu_bc15_spmd_sse.cpp
    encoder/basisu_opencl.cpp
    encoder/pvpngreader.cpp
    encoder/basisu_uastc_hdr_4x4_enc.cpp
    encoder/basisu_astc_hdr_6x6_enc.cpp
    encoder/basisu_astc_hdr_common.cpp
    encoder/basisu_astc_ldr_common.cpp
    encoder/basisu_astc_ldr_encode.cpp
	encoder/basisu_astc_ldr_fencode.cpp
    encoder/basisu_xbc7_encode.cpp
    encoder/basisu_tinyexr.cpp
    transcoder/basisu_transcoder.cpp
    encoder/basisu_astc_hdr_6x6_enc.h
    encoder/basisu_astc_hdr_common.h
    encoder/basisu_backend.h
    encoder/basisu_basis_file.h
    encoder/basisu_bc7enc.h
    encoder/basisu_comp.h
    encoder/basisu_enc.h
    encoder/basisu_etc.h
    encoder/basisu_frontend.h
    encoder/basisu_gpu_texture.h
    encoder/basisu_kernels_declares.h
    encoder/basisu_kernels_imp.h
    encoder/basisu_math.h
    encoder/basisu_miniz.h
    encoder/basisu_ocl_kernels.h
    encoder/basisu_opencl.h
    encoder/basisu_pvrtc1_4.h
    encoder/basisu_resampler_filters.h
    encoder/basisu_resampler.h
    encoder/basisu_ssim.h
    encoder/basisu_uastc_enc.h
    encoder/basisu_bc7e_scalar.h
    encoder/basisu_dds_export.h
    encoder/basisu_uastc_hdr_4x4_enc.h
    encoder/basisu_astc_ldr_common.h
    encoder/basisu_astc_ldr_encode.h
	encoder/basisu_astc_ldr_fencode.h
    encoder/basisu_xbc7_encode.h
    encoder/cppspmd_flow.h
    encoder/cppspmd_math_declares.h
    encoder/cppspmd_math.h
    encoder/cppspmd_sse.h
    encoder/cppspmd_type_aliases.h
    encoder/jpgd.h
    encoder/pvpngreader.h
    encoder/3rdparty/qoi.h
    encoder/3rdparty/tinyexr.h
    transcoder/basisu_astc_hdr_core.h
    transcoder/basisu_astc_helpers.h
    transcoder/basisu_containers_impl.h
    transcoder/basisu_containers.h
    transcoder/basisu_file_headers.h
    transcoder/basisu_transcoder_internal.h
    transcoder/basisu_transcoder_uastc.h
    transcoder/basisu_transcoder.h
    transcoder/basisu_idct.h
    transcoder/basisu.h
    zstd/zstd.h
)

if (NOT BASISU_DISABLE_ANDROID_ASTC_DECOMP)
    list(APPEND ENCODER_LIB_SRC_LIST
        encoder/3rdparty/android_astc_decomp.cpp
    )
endif()

if (BASISU_SUPPORT_ASTCENC)
    list(APPEND ENCODER_LIB_SRC_LIST
        encoder/3rdparty/astc-encoder-main/Source/astcenc_averages_and_directions.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_block_sizes.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_color_quantize.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_color_unquantize.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_compress_symbolic.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_compute_variance.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_decompress_symbolic.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_diagnostic_trace.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_entry.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_find_best_partitioning.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_ideal_endpoints_and_weights.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_image.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_integer_sequence.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_mathlib.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_mathlib_softfloat.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_partition_tables.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_percentile_tables.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_pick_best_endpoint_format.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_quantization.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_symbolic_physical.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_weight_align.cpp
        encoder/3rdparty/astc-encoder-main/Source/astcenc_weight_quant_xfer_tables.cpp
    )
endif()

if (BASISU_ZSTD)
    set(ENCODER_LIB_SRC_LIST ${ENCODER_LIB_SRC_LIST} zstd/zstd.c)
endif()

# Create the static library
add_library(basisu_encoder STATIC ${ENCODER_LIB_SRC_LIST})

if (BASISU_SUPPORT_ASTCENC)
    target_compile_definitions(basisu_encoder PRIVATE BASISU_SUPPORT_ASTCENC=1)
else()
    target_compile_definitions(basisu_encoder PRIVATE BASISU_SUPPORT_ASTCENC=0)
endif()

# Create the basisu executable and link against the static library
add_executable(basisu basisu_tool.cpp basisu_text_image.cpp basisu_tool_test_codecs.inl)
target_link_libraries(basisu PRIVATE basisu_encoder)

# Create the new example executable and link against the static library
if(BASISU_EXAMPLES)
    add_executable(example example/example.cpp)
    target_link_libraries(example PRIVATE basisu_encoder)
    
    add_executable(example_capi example_capi/example_capi.c encoder/basisu_wasm_api.cpp encoder/basisu_wasm_transcoder_api.cpp)
    target_link_libraries(example_capi PRIVATE basisu_encoder)
    
    add_executable(example_transcoding example_transcoding/example_transcoding.cpp example_transcoding/utils.cpp zstd/zstddeclib.c transcoder/basisu_transcoder.cpp)
endif() 

if (BASISU_BUILD_WASM)
    # Add proper suffix
    set_target_properties(basisu PROPERTIES SUFFIX ".wasm")
    
    if(BASISU_EXAMPLES)
        set_target_properties(example PROPERTIES SUFFIX ".wasm")
        set_target_properties(example_capi PROPERTIES SUFFIX ".wasm")
        set_target_properties(example_transcoding PROPERTIES SUFFIX ".wasm")
    endif()

    if (BASISU_WASM_THREADING)
        # multithreaded: use "basisu_mt.wasm"
        set_target_properties(basisu PROPERTIES OUTPUT_NAME "basisu_mt")
        
        if(BASISU_EXAMPLES)
            set_target_properties(example PROPERTIES OUTPUT_NAME "example_mt")
            set_target_properties(example_capi PROPERTIES OUTPUT_NAME "example_capi_mt")
            set_target_properties(example_transcoding PROPERTIES OUTPUT_NAME "example_transcoding_mt")
        endif()
        
    else()
        # single-threaded: use "basisu_st.wasm"
        set_target_properties(basisu PROPERTIES OUTPUT_NAME "basisu_st")
        
        if(BASISU_EXAMPLES)
            set_target_properties(example PROPERTIES OUTPUT_NAME "example_st")
            set_target_properties(example_capi PROPERTIES OUTPUT_NAME "example_capi_st")
            set_target_properties(example_transcoding PROPERTIES OUTPUT_NAME "example_transcoding_st")
        endif()
        
    endif()

    # 256 MB initial, 3.5 GB max - safe defaults for BasisU
    target_link_options(basisu PRIVATE
        -Wl,--initial-memory=268435456
        -Wl,--max-memory=3758096384
    )

    if (BASISU_EXAMPLES)
        target_link_options(example PRIVATE
            -Wl,--initial-memory=268435456
            -Wl,--max-memory=3758096384
        )
        target_link_options(example_capi PRIVATE
            -Wl,--initial-memory=268435456
            -Wl,--max-memory=3758096384
        )
        target_link_options(example_transcoding PRIVATE
            -Wl,--initial-memory=268435456
            -Wl,--max-memory=3758096384
        )
    endif()
endif()

#if (MSVC)
    target_sources(basisu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/basisu.manifest")
    if(BASISU_EXAMPLES)
        target_sources(example PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/basisu.manifest")
        target_sources(example_capi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/basisu.manifest")
        target_sources(example_transcoding PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/basisu.manifest")
    endif()         
#endif()

if (BASISU_ZSTD)
    # Note: the static encoder library MUST be compiled with the same value, otherwise its TUs
    # (transcoder, basisu_comp, basisu_xbc7_encode, basisu_astc_ldr_encode) default to ZSTD=1 and
    # emit ZSTD_* calls that won't link when zstd.c is excluded.
    target_compile_definitions(basisu_encoder PRIVATE BASISD_SUPPORT_KTX2_ZSTD=1)
    target_compile_definitions(basisu PRIVATE BASISD_SUPPORT_KTX2_ZSTD=1)
    if(BASISU_EXAMPLES)
        target_compile_definitions(example PRIVATE BASISD_SUPPORT_KTX2_ZSTD=1)
        target_compile_definitions(example_capi PRIVATE BASISD_SUPPORT_KTX2_ZSTD=1)
        target_compile_definitions(example_transcoding PRIVATE BASISD_SUPPORT_KTX2_ZSTD=1)
    endif()
else()
    target_compile_definitions(basisu_encoder PRIVATE BASISD_SUPPORT_KTX2_ZSTD=0)
    target_compile_definitions(basisu PRIVATE BASISD_SUPPORT_KTX2_ZSTD=0)
    if(BASISU_EXAMPLES)
        target_compile_definitions(example PRIVATE BASISD_SUPPORT_KTX2_ZSTD=0)
        target_compile_definitions(example_capi PRIVATE BASISD_SUPPORT_KTX2_ZSTD=0)
        target_compile_definitions(example_transcoding PRIVATE BASISD_SUPPORT_KTX2_ZSTD=0)
    endif()
endif()

if (NOT MSVC)
    # For Non-Windows builds, let cmake try and find the system OpenCL headers/libs for us.
    if (BASISU_OPENCL AND OPENCL_FOUND)
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DBASISU_SUPPORT_OPENCL=1")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBASISU_SUPPORT_OPENCL=1")    
        
        target_include_directories(basisu PRIVATE ${OpenCL_INCLUDE_DIRS})
        if(BASISU_EXAMPLES)
            target_include_directories(example PRIVATE ${OpenCL_INCLUDE_DIRS})
            target_include_directories(example_capi PRIVATE ${OpenCL_INCLUDE_DIRS})
        endif()         
        target_include_directories(basisu_encoder PRIVATE ${OpenCL_INCLUDE_DIRS})
        set(BASISU_EXTRA_LIBS ${OpenCL_LIBRARIES})
    endif()
else()
    # For Windows builds, we use our local copies of the OpenCL import lib and Khronos headers.
    if (BASISU_OPENCL)
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DBASISU_SUPPORT_OPENCL=1")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBASISU_SUPPORT_OPENCL=1")    
        
        target_include_directories(basisu PRIVATE "OpenCL")
        if(BASISU_EXAMPLES)
            target_include_directories(example PRIVATE "OpenCL")
            target_include_directories(example_capi PRIVATE "OpenCL")
        endif()
        target_include_directories(basisu_encoder PRIVATE "OpenCL")

        if (BASISU_BUILD_X64)
            target_link_libraries(basisu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/OpenCL/lib/OpenCL64.lib")
            if(BASISU_EXAMPLES)
                target_link_libraries(example PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/OpenCL/lib/OpenCL64.lib")
                target_link_libraries(example_capi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/OpenCL/lib/OpenCL64.lib")
            endif()
        else()
            target_link_libraries(basisu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/OpenCL/lib/OpenCL.lib")
            if(BASISU_EXAMPLES)
                target_link_libraries(example PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/OpenCL/lib/OpenCL.lib")
                target_link_libraries(example_capi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/OpenCL/lib/OpenCL.lib")
            endif()
        endif()
    endif()
endif()    

if (NOT MSVC AND NOT BASISU_BUILD_WASM)
    target_link_libraries(basisu PRIVATE m pthread ${BASISU_EXTRA_LIBS})
    if(BASISU_EXAMPLES)
        target_link_libraries(example PRIVATE m pthread ${BASISU_EXTRA_LIBS})
        target_link_libraries(example_capi PRIVATE m pthread ${BASISU_EXTRA_LIBS})
        target_link_libraries(example_transcoding PRIVATE m pthread ${BASISU_EXTRA_LIBS})
    endif()
endif()

if (NOT EMSCRIPTEN)
    if (UNIX AND NOT BASISU_BUILD_WASM)
        if (CMAKE_BUILD_TYPE STREQUAL Release)
            if (APPLE)
                add_custom_command(TARGET basisu POST_BUILD COMMAND strip -X -x ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/basisu)
                #message("strip command: strip -X -x ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/basisu")
            else()
                add_custom_command(TARGET basisu POST_BUILD COMMAND strip -g -X -x ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/basisu)
                #message("strip command: strip -g -X -x ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/basisu")
            endif()
        endif()
    endif()
endif()

if (MSVC)
    set_target_properties(basisu PROPERTIES 
        RUNTIME_OUTPUT_NAME "basisu"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )   
    
    if(BASISU_EXAMPLES)
        set_target_properties(example PROPERTIES 
            RUNTIME_OUTPUT_NAME "example"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )   
    endif()
    
    if(BASISU_EXAMPLES)
        set_target_properties(example_capi PROPERTIES 
            RUNTIME_OUTPUT_NAME "example_capi"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )   
    endif()
    
    if(BASISU_EXAMPLES)
        set_target_properties(example_transcoding PROPERTIES 
            RUNTIME_OUTPUT_NAME "example_transcoding"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )   
    endif()
endif()

# ------------------------------------------------------------
# Build WASM WASI executables (single or multi-threaded)
# ------------------------------------------------------------
if (BASISU_BUILD_WASM)
    target_compile_options(basisu_encoder PRIVATE -fno-exceptions -fno-rtti)
    target_compile_options(basisu        PRIVATE -fno-exceptions -fno-rtti)

    if (BASISU_EXAMPLES)
        target_compile_options(example PRIVATE -fno-exceptions -fno-rtti)
        target_compile_options(example_capi PRIVATE -fno-exceptions -fno-rtti)
        target_compile_options(example_transcoding PRIVATE -fno-exceptions -fno-rtti)
    endif()
endif()

if (BASISU_BUILD_WASM AND BASISU_WASM_THREADING)
    target_compile_definitions(basisu_encoder PRIVATE BASISU_WASI_THREADS=1)
    target_compile_definitions(basisu        PRIVATE BASISU_WASI_THREADS=1)

    if (BASISU_EXAMPLES)
        target_compile_definitions(example PRIVATE BASISU_WASI_THREADS=1)
        target_compile_definitions(example_capi PRIVATE BASISU_WASI_THREADS=1)
        target_compile_definitions(example_transcoding PRIVATE BASISU_WASI_THREADS=1)
    endif()
endif()

# ------------------------------------------------------------
# Build WASM WASI API module (single or multi-threaded)
# ------------------------------------------------------------
if (BASISU_BUILD_WASM)
    set(BASISU_WASM_API_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/encoder/basisu_wasm_api.cpp
    )

    # Select output name based on threading flag
    if (BASISU_WASM_THREADING)
        set(BASISU_WASM_OUTPUT_NAME "basisu_module_mt")
    else()
        set(BASISU_WASM_OUTPUT_NAME "basisu_module_st")
    endif()

    add_executable(${BASISU_WASM_OUTPUT_NAME} ${BASISU_WASM_API_SRC})
    target_link_libraries(${BASISU_WASM_OUTPUT_NAME} PRIVATE basisu_encoder)
    
    set_target_properties(${BASISU_WASM_OUTPUT_NAME} PROPERTIES SUFFIX ".wasm")

    # Common WASM options
    target_link_options(${BASISU_WASM_OUTPUT_NAME} PRIVATE
        -Wl,--initial-memory=268435456
        -Wl,--max-memory=3758096384
        -Wl,--stack-first
        -Wl,-z,stack-size=8388608
    )
    target_compile_options(${BASISU_WASM_OUTPUT_NAME} PRIVATE -fno-exceptions -fno-rtti)
    
    set_target_properties(${BASISU_WASM_OUTPUT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py/wasm
    )

    # Threading options
    if (BASISU_WASM_THREADING)
        target_compile_options(${BASISU_WASM_OUTPUT_NAME} PRIVATE
            -pthread
            -matomics
        )
        target_link_options(${BASISU_WASM_OUTPUT_NAME} PRIVATE
            -pthread
            -Wl,--shared-memory
            -Wl,--export-memory
        )

        target_compile_definitions(${BASISU_WASM_OUTPUT_NAME} PRIVATE BASISU_WASI_THREADS=1)
    endif()

endif()

if (BASISU_BUILD_WASM)
    # Select output name based on threading flag
    if (BASISU_WASM_THREADING)
        set(BASISU_TRANSCODER_WASM_OUTPUT_NAME "basisu_transcoder_module_mt")
    else()
        set(BASISU_TRANSCODER_WASM_OUTPUT_NAME "basisu_transcoder_module_st")
    endif()

    add_executable(${BASISU_TRANSCODER_WASM_OUTPUT_NAME} 
        ${CMAKE_CURRENT_SOURCE_DIR}/encoder/basisu_wasm_transcoder_api.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transcoder/basisu_transcoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/zstddeclib.c)
            
    set_target_properties(${BASISU_TRANSCODER_WASM_OUTPUT_NAME} PROPERTIES SUFFIX ".wasm")

    target_link_options(${BASISU_TRANSCODER_WASM_OUTPUT_NAME} PRIVATE
        -Wl,--initial-memory=16777216
        -Wl,--stack-first
        -Wl,-z,stack-size=4194304
    )
    target_compile_options(${BASISU_TRANSCODER_WASM_OUTPUT_NAME} PRIVATE -fno-exceptions -fno-rtti)
    
    set_target_properties(${BASISU_TRANSCODER_WASM_OUTPUT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py/wasm
    )
endif()


# ------------------------------------------------------------
# Optional: Build native Python modules (pybind11)
# ------------------------------------------------------------
if (BASISU_BUILD_PYTHON AND NOT BASISU_BUILD_WASM)
    find_package(pybind11 CONFIG REQUIRED)

    message(STATUS "Building pybind11 Python extension: basisu_python")

    pybind11_add_module(basisu_python
        python/basisu_encoder_pybind11.cpp
        encoder/basisu_wasm_api.cpp    
    )
    
    # Ensure PIC ONLY for this target
    set_property(TARGET basisu_python PROPERTY POSITION_INDEPENDENT_CODE ON)

    target_link_libraries(basisu_python PRIVATE basisu_encoder)

    # Put basisu_python so into python/basisu_py
    set_target_properties(basisu_python PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
        PREFIX ""                      # Required by Python
        OUTPUT_NAME "basisu_python"    # Just to be explicit
    )
    
    if (MSVC)
        set_target_properties(basisu_python PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_RELEASE       ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
            LIBRARY_OUTPUT_DIRECTORY_DEBUG         ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL    ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
            SUFFIX ".pyd"
        )
    endif()

endif()

if (BASISU_BUILD_PYTHON AND NOT BASISU_BUILD_WASM)
    find_package(pybind11 CONFIG REQUIRED)
    
    message(STATUS "Building pybind11 Python extension: basisu_transcoder_python")

    pybind11_add_module(basisu_transcoder_python
        python/basisu_transcoder_pybind11.cpp
        encoder/basisu_wasm_transcoder_api.cpp  
        transcoder/basisu_transcoder.cpp
        zstd/zstddeclib.c
    )
    
    # Ensure PIC ONLY for this target
    set_property(TARGET basisu_transcoder_python PROPERTY POSITION_INDEPENDENT_CODE ON)
        
    set_target_properties(basisu_transcoder_python PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
        PREFIX ""
        OUTPUT_NAME "basisu_transcoder_python"
    )
    
    if (MSVC)
        set_target_properties(basisu_transcoder_python PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_RELEASE       ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
            LIBRARY_OUTPUT_DIRECTORY_DEBUG         ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL    ${CMAKE_CURRENT_SOURCE_DIR}/python/basisu_py
            SUFFIX ".pyd"
        )
    endif()
endif()
