#include "profiler.h"

#include <dlib/dlib.h>
#include <dlib/profile.h>
#include <dlib/log.h>
#include <extension/extension.h>
#include <render/render.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "profiler_private.h"
#include "profile_render.h"

namespace dmProfiler
{

/*# Profiler API documentation
 *
 * Functions for getting profiling data in runtime.
 * More detailed profiling and debugging information can be found under the [Debugging](http://www.defold.com/manuals/debugging/) section in the manuals.
 *
 * @document
 * @name Profiler
 * @namespace profiler
 */

static bool g_TrackCpuUsage = false;
static dmProfileRender::HRenderProfile gRenderProfile = 0;
static uint32_t gUpdateFrequency = 60;

void SetUpdateFrequency(uint32_t update_frequency)
{
    gUpdateFrequency = update_frequency;
}

void ToggleProfiler()
{
    if (gRenderProfile)
    {
        dmProfileRender::DeleteRenderProfile(gRenderProfile);
        gRenderProfile = 0;
    }
    else
    {
        gRenderProfile = dmProfileRender::NewRenderProfile(gUpdateFrequency);
    }
}

void RenderProfiler(dmProfile::HProfile profile, dmGraphics::HContext graphics_context, dmRender::HRenderContext render_context, dmRender::HFontMap system_font_map)
{
    if(gRenderProfile)
    {
        DM_PROFILE(Profile, "Draw");
        dmProfile::Pause(true);

        dmProfileRender::UpdateRenderProfile(gRenderProfile, profile);
        dmRender::RenderListBegin(render_context);
        dmProfileRender::Draw(gRenderProfile, render_context, system_font_map);
        dmRender::RenderListEnd(render_context);
        dmRender::SetViewMatrix(render_context, Vectormath::Aos::Matrix4::identity());
        dmRender::SetProjectionMatrix(render_context, Vectormath::Aos::Matrix4::orthographic(0.0f, dmGraphics::GetWindowWidth(graphics_context), 0.0f, dmGraphics::GetWindowHeight(graphics_context), 1.0f, -1.0f));
        dmRender::DrawRenderList(render_context, 0, 0);
        dmRender::ClearRenderObjects(render_context);

        dmProfile::Pause(false);
    }
}

void ShutdownRenderProfile()
{
    if (gRenderProfile)
    {
        dmProfileRender::DeleteRenderProfile(gRenderProfile);
        gRenderProfile = 0;
    }
}

/*# get current memory usage for app reported by OS
 * Get the amount of memory used (resident/working set) by the application in bytes, as reported by the OS.
 *
 * [icon:attention] This function is not available on [icon:html5] HTML5.
 *
 * The values are gathered from internal OS functions which correspond to the following;
 *
 * OS                                | Value
 * ----------------------------------|------------------
 * [icon:ios] iOS<br/>[icon:macos] MacOS<br/>[icon:android]<br/>Androd<br/>[icon:linux] Linux | [Resident memory](https://en.wikipedia.org/wiki/Resident_set_size)
 * [icon:windows] Windows            | [Working set](https://en.wikipedia.org/wiki/Working_set)
 * [icon:html5] HTML5                | [icon:attention] Not available
 *
 * @name profiler.get_memory_usage
 * @return bytes [type:number] used by the application
 * @examples
 *
 * Get memory usage before and after loading a collection:
 *
 * ```lua
 * print(profiler.get_memory_usage())
 * msg.post("#collectionproxy", "load")
 * ...
 * print(profiler.get_memory_usage()) -- will report a higher number than the initial call
 * ```
 */
static int MemoryUsage(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1)
    lua_pushnumber(L, dmProfilerExt::GetMemoryUsage());
    return 1;
}

/*# get current CPU usage for app reported by OS
 * Get the percent of CPU usage by the application, as reported by the OS.
 *
 * [icon:attention] This function is not available on [icon:html5] HTML5.
 *
 * For some platforms ([icon:android] Android, [icon:linux] Linux and [icon:windows] Windows), this information is only available
 * by default in the debug version of the engine. It can be enabled in release version as well
 * by checking `track_cpu` under `profiler` in the `game.project` file.
 * (This means that the engine will sample the CPU usage in intervalls during execution even in release mode.)
 *
 * @name profiler.get_cpu_usage
 * @return percent [type:number] of CPU used by the application
 */
static int CPUUsage(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1)
    lua_pushnumber(L, dmProfilerExt::GetCpuUsage());
    return 1;
}

/*# enables or disables the on-screen profiler ui
 * Creates and shows or hides and destroys the on-sceen profiler ui
 * 
 * The profiler is a real-time tool that shows the numbers of milliseconds spent
 * in each scope per frame as well as counters. The profiler is very useful for
 * tracking down performance and resource problems.
 * 
 * @name profiler.enable_ui
 * @param enabled [type:boolean] true to enable, false to disable
 * 
 * @examples
 * ```lua
 *      profiler.enable_ui(true)
 * ```
 */
static int EnableProfilerUI(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0)

    if (!lua_isboolean(L, 1))
    {
        return DM_LUA_ERROR("Invalid parameter, expected a boolean but got a %s", lua_typename(L, lua_type(L, 1)))
    }

    bool enabled = lua_toboolean(L, 1);

    if (enabled && !gRenderProfile)
    {
        gRenderProfile = dmProfileRender::NewRenderProfile(gUpdateFrequency);
    }
    else if (!enabled && gRenderProfile)
    {
        dmProfileRender::DeleteRenderProfile(gRenderProfile);
        gRenderProfile = 0;
    }

    return 0;
}

/*# sets the the on-screen profiler ui mode
 * Set the on-screen profile mode - run, pause, record or show peak frame
 * 
 * @name profiler.set_ui_mode
 * @param mode [type:constant] the mode to set the ui profiler in
 * 
 * - `profiler.MODE_RUN` This is default mode that continously shows the last frame
 * - `profiler.MODE_PAUSE` Pauses on the currently displayed frame
 * - `profiler.MODE_SHOW_PEAK_FRAME` Pauses on the currently displayed frame but shows a new frame if that frame is slower 
 * - `profiler.MODE_RECORD` Records all incoming frames to the recording buffer
 * 
 * To stop recording, switch to a different mode such as `MODE_PAUSE` or `MODE_RUN`.
 * You can also use the `view_recorded_frame` function to display a recorded frame. Doing so stops the recording as well.
 * 
 * Every time you switch to recording mode the recording buffer is cleared.
 * The recording buffer is also cleared when setting the `MODE_SHOW_PEAK_FRAME` mode.
 * 
 * @examples
 * ```lua
 * function start_recording()
 *      profiler.set_ui_mode(profiler.MODE_RECORD)
 * end
 * 
 * function stop_recording()
 *      profiler.set_ui_mode(profiler.MODE_PAUSE)
 * end
 * ```
 */
static int SetProfileUIMode(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0)

    if (!gRenderProfile)
    {
        return 0;
    }

    uint32_t mode = luaL_checknumber(L, 1);

    dmProfileRender::SetMode(gRenderProfile, (dmProfileRender::ProfilerMode)mode);

    return 0;
}

/*# sets the the on-screen profiler ui view mode
 * Set the on-screen profile view mode - minimized or expanded
 * 
 * @name profiler.set_ui_view_mode
 * @param mode [type:constant] the view mode to set the ui profiler in
 * 
 * - `profiler.VIEW_MODE_FULL` The default mode which displays all the ui profiler details
 * - `profiler.VIEW_MODE_MINIMIZED` Minimized mode which only shows the top header (fps counters and ui profiler mode)
 * 
 * 
 * @examples
 * ```lua
 *      profiler.set_ui_view_mode(profiler.VIEW_MODE_MINIMIZED)
 * ```
 */
static int SetProfilerUIViewMode(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0)

    if (!gRenderProfile)
    {
        return 0;
    }

    uint32_t mode = luaL_checknumber(L, 1);

    dmProfileRender::SetViewMode(gRenderProfile, (dmProfileRender::ProfilerViewMode)mode);

    return 0;
}

/*# Shows or hides the vsync wait time in the on-screen profiler ui
 * Shows or hides the time the engine waits for vsync in the on-screen profiler
 * 
 * Each frame the engine waits for vsync and depending on your vsync settings and how much time
 * your game logic takes this time can dwarf the time in the game logic making it hard to
 * see details in the on-screen profiler graph and lists.
 * 
 * Also, by hiding this the FPS times in the header show the time spent each time excuding the
 * time spent waiting for vsync. This shows you how long time your game is spending actively
 * working each frame.
 * 
 * This setting also effects the display of recorded frames but does not affect the actual
 * recorded frames so it is possible to toggle this on and off when viewing recorded frames.
 * 
 * By default the vsync wait times is displayed in the profiler.
 * 
 * @name profiler.set_ui_vsync_wait_visible
 * @param visible [type:boolean] true to include it in the display, false to hide it.
 * 
 * @examples
 * ```lua
 *      profiler.set_ui_vsync_wait_visible(false)
 * end
 * ```
 */
static int SetProfileUIVSyncWaitVisible(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0)

    if (!gRenderProfile)
    {
        return 0;
    }

    if (!lua_isboolean(L, 1))
    {
        return DM_LUA_ERROR("Invalid parameter, expected a boolean but got a %s", lua_typename(L, lua_type(L, 1)))
    }

    bool visible = lua_toboolean(L, 1);

    dmProfileRender::SetWaitTime(gRenderProfile, visible);

    return 0;
}

/*# get the number of recorded frames in the on-screen profiler ui
 * Get the number of recorded frames in the on-screen profiler ui recording buffer
 * 
 * @name profiler.recorded_frame_count
 * @return [type:number] the number of recorded frames, zero if on-screen profiler is disabled
 * 
 * @examples
 * ```lua
 *      -- Show the last recorded frame
 *      local recorded_frame_count = profiler.recorded_frame_count()
 *      profiler.view_recorded_frame(recorded_frame_count)
 * end
 * ```
 */
static int ProfilerUIRecordedFrameCount(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1)

    if (!gRenderProfile)
    {
        lua_pushnumber(L, 0);
    }
    else
    {
        int frame_count = dmProfileRender::GetRecordedFrameCount(gRenderProfile);
        lua_pushnumber(L, frame_count);
    }
    
    return 1;
}

/*# displays a previously recorded frame in the on-screen profiler ui
 * Pauses and displays a frame from the recording buffer in the on-screen profiler ui
 * 
 * The frame to show can either be an absolute frame or a relative frame to the current frame.
 * 
 * @name profiler.view_recorded_frame
 * @param frame_index [type:table] a table where you specify one of the following parameters:
 * 
 * - `distance` The offset from the currently displayed frame (this is truncated between zero and the number of recorded frames)
 * - `frame` The frame index in the recording buffer (1 is first recorded frame)
 * 
 * @examples
 * ```lua
 *      profiler.view_recorded_frame({distance = -1})
 * end
 * ```
 */
static int ProfilerUIViewRecordedFrame(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0)

    if (!gRenderProfile)
    {
        return 0;
    }

    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, -1, "distance");
    int distance = lua_isnil(L, -1) ? 0 : luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "frame");
    int frame = lua_isnil(L, -1) ? -1 : luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    if (distance != 0)
    {
        dmProfileRender::AdjustShownFrame(gRenderProfile, distance);
    }
    else if (frame != -1)
    {
        int recorded_frame_count = dmProfileRender::GetRecordedFrameCount(gRenderProfile);
        if (recorded_frame_count == 0)
        {
            return DM_LUA_ERROR("The profiler recording buffer is empty");
        }
        if (frame < 1 || frame > recorded_frame_count)
        {
            return DM_LUA_ERROR("Frame index is out of range, valid range is %d to %d", 1, recorded_frame_count);
        }
        dmProfileRender::ShowRecordedFrame(gRenderProfile, frame - 1);
    }
    return DM_LUA_ERROR("'distance' or 'frame' must be given in properties table");
}

} // dmProfiler

static dmExtension::Result InitializeProfiler(dmExtension::Params* params)
{
    // Should CPU sample tracking be run each step?
    // This is enabled by default in debug, but can be turned on in release via project config.
    dmProfiler::g_TrackCpuUsage = dLib::IsDebugMode();
    if (dmConfigFile::GetInt(params->m_ConfigFile, "profiler.track_cpu", 0) == 1)
    {
        dmProfiler::g_TrackCpuUsage = true;
    }

    static const luaL_reg Module_methods[] =
    {
        {"get_memory_usage", dmProfiler::MemoryUsage},
        {"get_cpu_usage", dmProfiler::CPUUsage},
        {"enable_ui", dmProfiler::EnableProfilerUI},
        {"set_ui_mode", dmProfiler::SetProfileUIMode},
        {"set_ui_view_mode", dmProfiler::SetProfilerUIViewMode},
        {"set_ui_vsync_wait_visible", dmProfiler::SetProfileUIVSyncWaitVisible},
        {"recorded_frame_count", dmProfiler::ProfilerUIRecordedFrameCount},
        {"view_recorded_frame", dmProfiler::ProfilerUIViewRecordedFrame},
        {0, 0}
    };

    luaL_register(params->m_L, "profiler", Module_methods);

    lua_pushnumber(params->m_L, (lua_Number) dmProfileRender::PROFILER_MODE_RUN);
    lua_setfield(params->m_L, -2, "MODE_RUN");
    lua_pushnumber(params->m_L, (lua_Number) dmProfileRender::PROFILER_MODE_PAUSE);
    lua_setfield(params->m_L, -2, "MODE_PAUSE");
    lua_pushnumber(params->m_L, (lua_Number) dmProfileRender::PROFILER_MODE_SHOW_PEAK_FRAME);
    lua_setfield(params->m_L, -2, "MODE_SHOW_PEAK_FRAME");
    lua_pushnumber(params->m_L, (lua_Number) dmProfileRender::PROFILER_MODE_RECORD);
    lua_setfield(params->m_L, -2, "MODE_RECORD");

    lua_pushnumber(params->m_L, (lua_Number) dmProfileRender::PROFILER_VIEW_MODE_FULL);
    lua_setfield(params->m_L, -2, "VIEW_MODE_FULL");
    lua_pushnumber(params->m_L, (lua_Number) dmProfileRender::PROFILER_VIEW_MODE_MINIMIZED);
    lua_setfield(params->m_L, -2, "VIEW_MODE_MINIMIZED");

    lua_pop(params->m_L, 1);

    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateProfiler(dmExtension::Params* params)
{
    if (dmProfiler::g_TrackCpuUsage)
    {
        dmProfilerExt::SampleCpuUsage();
    }

    if (dLib::IsDebugMode()) {
        DM_COUNTER("CPU Usage", dmProfilerExt::GetCpuUsage()*100.0);
        DM_COUNTER("Mem Usage (Kb)", dmProfilerExt::GetMemoryUsage() / 1024u);
    }

    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeProfiler(dmExtension::Params* params)
{
    dmProfiler::ShutdownRenderProfile();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppInitializeProfiler(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeProfiler(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ProfilerExt, "Profiler", AppInitializeProfiler, AppFinalizeProfiler, InitializeProfiler, UpdateProfiler, 0, FinalizeProfiler)
