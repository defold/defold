# Out-of-tree iOS embed hosts (e.g. defold-as-a-library) link against a built
# dmengine_embed + the same static graph as a normal dmengine debug binary.
#
# Required cache/env when calling defold_configure_ios_embed_host():
#   DEFOLD_ROOT          — engine checkout
#   DEFOLD_BUILD_HOME    — usually ${DEFOLD_ROOT}/engine
#   TARGET_PLATFORM      — arm64-ios (or x86_64-ios)
#   DEFOLD_SDK_ROOT or DYNAMO_HOME — installed/ext libs (dmglfw, luajit, …)
#
# Does not know sample app names or Swift sources — callers own add_executable().

function(defold_configure_ios_embed_host target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "defold_configure_ios_embed_host: target '${target}' does not exist")
  endif()
  if(NOT DEFOLD_ROOT OR NOT EXISTS "${DEFOLD_ROOT}/engine/engine/src/embed/defold_embed.h")
    message(FATAL_ERROR "defold_configure_ios_embed_host: DEFOLD_ROOT must point at a Defold checkout with embed headers")
  endif()
  if(NOT DEFOLD_BUILD_HOME)
    message(FATAL_ERROR "defold_configure_ios_embed_host: DEFOLD_BUILD_HOME is required")
  endif()
  if(NOT TARGET_PLATFORM)
    message(FATAL_ERROR "defold_configure_ios_embed_host: TARGET_PLATFORM is required")
  endif()

  set(_sdk "${DEFOLD_SDK_ROOT}")
  if(NOT _sdk)
    set(_sdk "$ENV{DYNAMO_HOME}")
  endif()
  if(NOT _sdk AND DEFINED DYNAMO_HOME)
    set(_sdk "${DYNAMO_HOME}")
  endif()
  if(NOT _sdk)
    message(FATAL_ERROR "defold_configure_ios_embed_host: set DEFOLD_SDK_ROOT or DYNAMO_HOME")
  endif()

  set(_p "${TARGET_PLATFORM}")
  # Engine CMake places per-lib build trees under ${DEFOLD_BUILD_HOME}/engine/<lib>/build/<platform>.
  set(_bh "${DEFOLD_BUILD_HOME}/engine")
  set(_embed_inc "${DEFOLD_ROOT}/engine/engine/src/embed")

  set(_engine_libs
    "${_bh}/engine/build/${_p}/libdmengine_embed.a"
    "${_bh}/engine/build/${_p}/libengine.a"
    "${_bh}/engine/build/${_p}/libengine_service.a"
    "${_bh}/dlib/build/${_p}/libprofile.a"
    "${_bh}/profiler/build/${_p}/libprofilerext.a"
    "${_bh}/profiler/build/${_p}/libprofiler_remotery.a"
    "${_bh}/gameobject/build/${_p}/libgameobject.a"
    "${_bh}/ddf/build/${_p}/libddf.a"
    "${_bh}/liveupdate/build/${_p}/libliveupdate.a"
    "${_bh}/resource/build/${_p}/libresource.a"
    "${_bh}/gamesys/build/${_p}/libgamesys.a"
    "${_bh}/gamesys/build/${_p}/libgamesys_model.a"
    "${_bh}/gamesys/build/${_p}/libgamesys_rig.a"
    "${_bh}/gamesys/build/${_p}/libscript_box2d_defold.a"
    "${_bh}/render/build/${_p}/librender.a"
    "${_bh}/render/build/${_p}/librender_font_default.a"
    "${_bh}/script/build/${_p}/libscript.a"
    "${_bh}/extension/build/${_p}/libextension.a"
    "${_bh}/input/build/${_p}/libinput.a"
    "${_bh}/particle/build/${_p}/libparticle.a"
    "${_bh}/rig/build/${_p}/librig.a"
    "${_bh}/dlib/build/${_p}/libdlib.a"
    "${_bh}/dlib/build/${_p}/libzip.a"
    "${_bh}/dlib/build/${_p}/libdmbedtls.a"
    "${_bh}/gui/build/${_p}/libgui.a"
    "${_bh}/crash/build/${_p}/libcrashext.a"
    "${_bh}/font/build/${_p}/libfont.a"
    "${_bh}/physics/build/${_p}/libphysics.a"
    "${_bh}/record/build/${_p}/librecord_null.a"
    "${_bh}/hid/build/${_p}/libhid.a"
    "${_bh}/sound/build/${_p}/libsound.a"
    "${_bh}/sound/build/${_p}/libdecoder_wav.a"
    "${_bh}/sound/build/${_p}/libdecoder_ogg.a"
    "${_bh}/platform/build/${_p}/libplatform.a"
    "${_bh}/graphics/build/${_p}/libgraphics_opengles.a"
    "${_bh}/dlib/build/${_p}/libimage.a"
    "${_bh}/graphics/build/${_p}/libgraphics_transcoder_basisu.a"
    "${_bh}/dlib/build/${_p}/libbasis_transcoder.a"
    "${_bh}/graphics/build/${_p}/libgraphics_metal.a"
    "${_bh}/graphics/build/${_p}/libgraphics_null.a"
    "${_bh}/platform/build/${_p}/libplatform_null.a"
    "${_bh}/graphics/build/${_p}/libgraphics.a"
    "${_bh}/graphics/build/${_p}/libgraphics_proto.a"
  )

  foreach(_lib IN LISTS _engine_libs)
    if(NOT EXISTS "${_lib}")
      message(FATAL_ERROR "defold_configure_ios_embed_host: missing ${_lib} — build dmengine_embed first")
    endif()
  endforeach()

  target_include_directories(${target} PRIVATE "${_embed_inc}")
  target_compile_definitions(${target} PRIVATE
    $<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:DM_ENGINE_EMBED=1>)
  target_link_directories(${target} PRIVATE
    "${_sdk}/lib/${_p}"
    "${_sdk}/ext/lib/${_p}")
  target_link_libraries(${target} PRIVATE
    ${_engine_libs}
    tremolo
    box2d_defold
    BulletDynamics
    BulletCollision
    LinearMath
    dmglfw
    luajit-5.1
    "-framework CFNetwork"
    "-framework Security"
    "-framework UIKit"
    "-framework SystemConfiguration"
    "-framework AVFoundation"
    "-weak_framework Foundation"
    "-framework CoreMotion"
    "-framework GameController"
    "-framework QuartzCore"
    "-framework OpenGLES"
    "-framework CoreVideo"
    "-framework CoreGraphics"
    "-framework Metal"
    "-framework MetalKit"
    "-framework IOSurface"
    "-framework AudioToolbox")
  target_link_options(${target} PRIVATE -stdlib=libc++ -fobjc-link-runtime -dead_strip)
  # Prefer clang++ for final link (swiftc rejects several Defold link flags).
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()
