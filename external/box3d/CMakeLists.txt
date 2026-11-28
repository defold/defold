# The upstream runtime is C17. Build it independently of the samples and tools.
file(GLOB BOX3D_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Box3D/src/*.c")
list(REMOVE_ITEM BOX3D_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/Box3D/src/convex_manifold.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Box3D/src/contact.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Box3D/src/contact_solver.c")
list(APPEND BOX3D_SOURCES defold/convex_contacts.c defold/contact.c defold/contact_solver.c)
set(BOX3D_TARGETS box3d)
if(TARGET_PLATFORM MATCHES "^(wasm|wasm_pthread)-web$")
  list(APPEND BOX3D_TARGETS box3d_nosimd)
endif()

foreach(target IN LISTS BOX3D_TARGETS)
  add_library(${target} STATIC ${BOX3D_SOURCES})
  target_compile_features(${target} PRIVATE c_std_17)
  target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Box3D/include" "${CMAKE_CURRENT_SOURCE_DIR}/defold/include")
  target_link_libraries(${target} PRIVATE defold_sdk)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU" AND NOT MSVC)
    target_compile_options(${target} PRIVATE -ffp-contract=off)
  endif()
  if(TARGET_PLATFORM MATCHES "win32$")
    set_target_properties(${target} PROPERTIES PREFIX "lib")
  endif()
endforeach()

if(TARGET box3d_nosimd)
  # Emscripten maps Box3D's four-lane SSE2 intrinsics to WebAssembly SIMD.
  target_compile_options(box3d PRIVATE -msimd128 -msse2)
  target_compile_definitions(box3d_nosimd PRIVATE BOX3D_DISABLE_SIMD)
  target_compile_options(box3d_nosimd PRIVATE -mno-simd128)
endif()
