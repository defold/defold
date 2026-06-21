defold_log("platform_xbox.cmake:")

set(DEFOLD_IS_PRIVATE_VENDOR ON CACHE BOOL "Building with private platform configuration" FORCE)
include(platform_xbone)

defold_get_private_repo_root(_DEFOLD_XBONE_PRIVATE_REPO_ROOT "${TARGET_PLATFORM}")
if(NOT _DEFOLD_XBONE_PRIVATE_REPO_ROOT AND DEFOLD_XBONE_PRIVATE_REPO_ROOT)
    set(_DEFOLD_XBONE_PRIVATE_REPO_ROOT "${DEFOLD_XBONE_PRIVATE_REPO_ROOT}")
endif()
set(DEFOLD_XBONE_PRIVATE_REPO_ROOT "${_DEFOLD_XBONE_PRIVATE_REPO_ROOT}" CACHE PATH "Private Xbox repository root" FORCE)

target_compile_definitions(defold_sdk INTERFACE
    DM_HOSTFS=\"G:\")

if(MSVC_CL)
    target_compile_definitions(defold_sdk INTERFACE _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(defold_sdk INTERFACE /FS)
endif()

set(DEFOLD_PLATFORM_GRAPHICS_SYMBOLS GraphicsAdapterDX12)
set(DEFOLD_PLATFORM_GRAPHICS_LIBS graphics_dx12 image graphics_transcoder_null d3d12_x xg_x)
set(DEFOLD_PLATFORM_HID_SOURCE_DIR "${_DEFOLD_XBONE_PRIVATE_REPO_ROOT}/engine/hid/src")
set(DEFOLD_PLATFORM_HID_DMSDK_DIRS "${_DEFOLD_XBONE_PRIVATE_REPO_ROOT}/engine/hid/src/dmsdk/hid/xbox")
set(DEFOLD_PLATFORM_TEST_DEFINES
    JC_TEST_NO_DEATH_TEST
    JC_TEST_USE_COLORS=0
    JC_TEST_USE_PRINTF)

target_link_libraries(defold_sdk INTERFACE
    Kernel32.lib
    Advapi32.lib
    WS2_32.lib
    Iphlpapi.lib
    Ole32.lib
    Bcrypt.lib
    xgameplatform.lib
    xgameruntime.lib
    xmem.lib
    Appnotify.lib
    crypt32.lib
    GameInput.lib
    Microsoft.Xbox.Services.142.C.lib
    PIXEvt.lib)
