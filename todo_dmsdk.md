# TODO: Move required private Defold APIs into dmsdk

## Private headers currently used by this repo

1) `dlib/job_thread.h` (private)
- Used in:
  - `defold-rive/pluginsrc/plugin.cpp`
  - `utils/viewer/viewer.cpp`
- Used APIs:
  - `dmJobThread::HContext`
  - `dmJobThread::JobThreadCreationParams`
  - `dmJobThread::Create(...)`
  - `dmJobThread::Destroy(...)`
  - `dmJobThread::Update(...)`

2) `graphics/graphics.h` (private)
- Used in:
  - `defold-rive/pluginsrc/plugin.cpp`
  - `defold-rive/pluginsrc/rive_jni.cpp`
  - `utils/viewer/viewer.cpp`
- Used APIs:
  - `dmGraphics::ContextParams`
  - `dmGraphics::NewContext(...)`
  - `dmGraphics::DeleteContext(...)`
  - `dmGraphics::InstallAdapter(...)`
  - `dmGraphics::BeginFrame(...)`
  - `dmGraphics::Flip(...)`
  - `dmGraphics::CloseWindow(...)`
  - `dmGraphics::Finalize()`

3) `platform/platform_window.h` (private)
- Used in:
  - `defold-rive/pluginsrc/plugin.cpp`
  - `utils/viewer/viewer.cpp`
- Used APIs:
  - `dmPlatform::HWindow`
  - `dmPlatform::WindowParams`
  - `dmPlatform::NewWindow()`
  - `dmPlatform::OpenWindow(...)`
  - `dmPlatform::ShowWindow(...)` (viewer)
  - `dmPlatform::IconifyWindow(...)` (plugin)
  - `dmPlatform::PollEvents(...)`

4) `gameobject/gameobject_ddf.h` (private)
- Used in:
  - `defold-rive/src/comp_rive.cpp`
- Used APIs:
  - `dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash`
  - `dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash`

## Plan to expose needed APIs in dmsdk

1) Job thread API
- Add public header: `engine/dlib/src/dmsdk/dlib/job_thread.h`.
- Expose:
  - `dmJobThread::HContext`
  - `dmJobThread::JobThreadCreationParams`
  - `dmJobThread::Create/Destroy/Update`
- Update dmsdk docs + any build/package lists to ship the header.

2) Graphics context + adapter lifecycle
- Extend `engine/graphics/src/dmsdk/graphics/graphics.h` (or add a new dmsdk header under `dmsdk/graphics/`) with:
  - `dmGraphics::ContextParams`
  - `dmGraphics::NewContext/DeleteContext`
  - `dmGraphics::InstallAdapter`
  - `dmGraphics::BeginFrame/Flip`
  - `dmGraphics::CloseWindow/Finalize`
- Ensure public docs cover threading restrictions and ownership rules.

3) Platform window API
- Add new public header: `engine/platform/src/dmsdk/platform/platform_window.h` (or similar).
- Expose:
  - `dmPlatform::HWindow`
  - `dmPlatform::WindowParams`
  - `dmPlatform::NewWindow/OpenWindow`
  - `dmPlatform::ShowWindow/IconifyWindow/PollEvents`
- Decide whether to keep this minimal (just window creation for tools) or full platform API.

4) Gameobject DDF message IDs
- Option A (minimal): add public constants/helpers in `dmsdk/gameobject/gameobject.h` for Enable/Disable message hashes.
- Option B: move `gameobject_ddf.h` to `engine/gameobject/src/dmsdk/gameobject/gameobject_ddf.h` and expose the DDF types.
- Update docs so extensions can safely check enable/disable messages without private headers.

5) Extension cleanup
- Update extension includes to use new dmsdk headers only.
- Remove private include paths from extension build scripts.
- Add CI check to forbid private header usage from extensions.
