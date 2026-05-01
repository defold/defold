// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "render_script.h"

#include <script/script.h>

namespace dmRender
{
    /*# Camera API documentation
     *
     * Camera functions, messages and constants.
     *
     * @document
     * @name Camera
     * @namespace camera
     * @language Lua
     */

    #define RENDER_SCRIPT_CAMERA_LIB_NAME "camera"

    /*# fixed orthographic zoom mode
     * Uses the manually set orthographic zoom value (camera.set_orthographic_zoom).
     *
     * @name camera.ORTHO_MODE_FIXED
     * @constant
     */

    /*# auto-fit orthographic zoom mode
     * Computes zoom so the original display area (game.project width/height) fits inside the window
     * while preserving aspect ratio. Equivalent to using min(window_width/width, window_height/height).
     *
     * @name camera.ORTHO_MODE_AUTO_FIT
     * @constant
     */

    /*# auto-cover orthographic zoom mode
     * Computes zoom so the original display area covers the entire window while preserving aspect ratio.
     * Equivalent to using max(window_width/width, window_height/height).
     *
     * @name camera.ORTHO_MODE_AUTO_COVER
     * @constant
     */

    struct RenderScriptCameraModule
    {
        dmRender::HRenderContext m_RenderContext;
    };

    static RenderScriptCameraModule g_RenderScriptCameraModule = { 0 };

    RenderCamera* CheckRenderCamera(lua_State* L, int index, HRenderContext render_context)
    {
        RenderCamera* camera = 0x0;

        if(lua_isnumber(L, index))
        {
            HRenderCamera h_camera = (HRenderCamera) lua_tonumber(L, index);
            camera = render_context->m_RenderCameras.Get(h_camera);

            if (!camera)
            {
                return (RenderCamera*) (uintptr_t) luaL_error(L, "Invalid handle.");
            }
        }
        else
        {
            dmMessage::URL url;
            if (dmScript::ResolveURL(L, index, &url, 0) != dmMessage::RESULT_OK)
            {
                return (RenderCamera*) (uintptr_t) luaL_error(L, "Could not resolve URL.");
            }

            camera = GetRenderCameraByUrl(g_RenderScriptCameraModule.m_RenderContext, url);
            if (camera == 0x0)
            {
                char buffer[256];
                dmScript::UrlToString(&url, buffer, sizeof(buffer));
                return (RenderCamera*) (uintptr_t) luaL_error(L, "Camera '%s' not found.", buffer);
            }
        }
        return camera;
    }

    // Resolve a camera from an optional argument. If no argument is given (or nil),
    // prefer the last enabled camera (matching default render script behavior).
    static RenderCamera* CheckRenderCameraOrDefault(lua_State* L, int index, HRenderContext render_context)
    {
        int top = lua_gettop(L);

        if (index <= top && !lua_isnil(L, index))
        {
            return CheckRenderCamera(L, index, render_context);
        }

        // Pick the last enabled camera (iterate from the end)
        for (int i = render_context->m_RenderCameras.Capacity() - 1; i >= 0; --i)
        {
            RenderCamera* camera = render_context->m_RenderCameras.GetByIndex(i);
            if (camera && camera->m_Enabled)
            {
                return camera;
            }
        }

        return (RenderCamera*) (uintptr_t) luaL_error(L, "No camera found.");
    }

    /*# Convert screen XY to world point at near plane
     * Converts 2D screen coordinates (x,y) to the 3D world-space point on the camera's near plane for that pixel.
     * If a camera isn't specified, the last enabled camera is used.
     *
     * @name camera.screen_xy_to_world
     * @param x [type:number] X coordinate on screen.
     * @param y [type:number] Y coordinate on screen.
     * @param [camera] [type:url|number|nil] optional camera id
     * @return world_pos [type:vector3] the world coordinate on the camera near plane
     *
     * @examples
     * Place objects at the touch point.
     *
     * ```lua
     *  function on_input(self, action_id, action)
     *      if action_id == hash("touch") then
     *          if action.pressed then
     *              local world_position = camera.screen_xy_to_world(action.screen_x, action.screen_y)
     *              go.set_position(world_position, "/go1")
     *          end
     *      end
     *  end
     * ```
     *
     */
    static int RenderScriptCamera_ScreenXYToWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        float sx = (float) luaL_checknumber(L, 1);
        float sy = (float) luaL_checknumber(L, 2);

        HRenderContext render_context = g_RenderScriptCameraModule.m_RenderContext;
        RenderCamera* camera = CheckRenderCameraOrDefault(L, 3, render_context);

        dmVMath::Vector3 world;
        float near_z = camera->m_Data.m_NearZ;
        const float eps = 0.0001;
        dmRender::Result r = dmRender::CameraScreenToWorld(render_context, camera->m_Handle, sx, sy, near_z + eps, &world);
        if (r != dmRender::RESULT_OK)
        {
            return DM_LUA_ERROR("Can't convert position (%d)", r);
        }
        dmScript::PushVector3(L, world);
        return 1;
    }

    /*# Convert screen vector3 position to world
     * Converts a screen-space 2D point with view depth to a 3D world point.
     * z is the view depth in world units measured from the camera plane along the camera forward axis.
     * If a camera isn't specified, the last enabled camera is used.
     *
     * @name camera.screen_to_world
     * @param pos [type:vector3] Screen-space position (x, y) with z as view depth in world units
     * @param [camera] [type:url|number|nil] optional camera id
     * @return world_pos [type:vector3] the world coordinate
     *
     * @examples
     * Place objects at the touch point with a random Z position, keeping them within the visible view zone.
     *
     * ```lua
     *  function on_input(self, action_id, action)
     *      if action_id == hash("touch") then
     *          if action.pressed then
     *              local percpective_camera = msg.url("#perspective_camera")
     *              local random_z = math.random(camera.get_near_z(percpective_camera) + 0.01, camera.get_far_z(percpective_camera) - 0.01)
     *              local world_position = camera.screen_to_world(vmath.vector3(action.screen_x, action.screen_y, random_z), percpective_camera)
     *              go.set_position(world_position, "/go1")
     *          end
     *      end
     *  end
     * ```
     *
     */
    static int RenderScriptCamera_ScreenToWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmVMath::Vector3* pos = dmScript::CheckVector3(L, 1);
        HRenderContext render_context = g_RenderScriptCameraModule.m_RenderContext;
        RenderCamera* camera = CheckRenderCameraOrDefault(L, 2, render_context);
        dmVMath::Vector3 world;
        dmRender::Result r = dmRender::CameraScreenToWorld(render_context, camera->m_Handle, pos->getX(), pos->getY(), pos->getZ(), &world);
        if (r != dmRender::RESULT_OK)
        {
            return DM_LUA_ERROR("Can't convert position (%d)", r);
        }
        dmScript::PushVector3(L, world);
        return 1;
    }

    /*# Convert world vector3 position to screen
     * Converts a 3D world position to screen-space coordinates with view depth.
     * Returns a vector3 where x and y are in screen pixels and z is the view depth in world units
     * measured from the camera plane along the camera forward axis. The returned z can be used with
     * camera.screen_to_world to reconstruct the world position on the same pixel ray.
     * If a camera isn't specified, the last enabled camera is used.
     *
     * @name camera.world_to_screen
     * @param world_pos [type:vector3] World-space position
     * @param [camera] [type:url|number|nil] optional camera id
     * @return screen_pos [type:vector3] Screen position (x,y in pixels, z is view depth)
     *
     * @examples
     * Convert go position into screen pisition
     *
     * ```lua
     *  go.update_world_transform("/go1")
     *  local world_pos = go.get_world_position("/go1")
     *  local screen_pos = camera.world_to_screen(world_pos)
     * ```
     *
     */
    static int RenderScriptCamera_WorldToScreen(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmVMath::Vector3* world = dmScript::CheckVector3(L, 1);
        HRenderContext render_context = g_RenderScriptCameraModule.m_RenderContext;
        RenderCamera* camera = CheckRenderCameraOrDefault(L, 2, render_context);

        dmVMath::Vector3 screen;
        dmRender::Result r = dmRender::CameraWorldToScreen(render_context, camera->m_Handle, *world, &screen);
        if (r != dmRender::RESULT_OK)
        {
            return DM_LUA_ERROR("Can't convert position (%d)", r);
        }
        dmScript::PushVector3(L, screen);
        return 1;
    }

    /*# get all camera URLs
    * This function returns a table with all the camera URLs that have been
    * registered in the render context.
    *
    * @name camera.get_cameras
    * @return cameras [type:table] a table with all camera URLs
    *
    * @examples
    * ```lua
    * for k,v in pairs(camera.get_cameras()) do
    *     render.set_camera(v)
    *     render.draw(...)
    *     render.set_camera()
    * end
    * ```
    */
    static int RenderScriptCamera_GetCameras(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        int camera_index = 1;

        lua_newtable(L);

        for (int i = 0; i < g_RenderScriptCameraModule.m_RenderContext->m_RenderCameras.Capacity(); ++i)
        {
            RenderCamera* camera = g_RenderScriptCameraModule.m_RenderContext->m_RenderCameras.GetByIndex(i);

            if (camera)
            {
                lua_pushinteger(L, camera_index);
                dmScript::PushURL(L, camera->m_URL);
                lua_settable(L, -3);
                camera_index++;
            }
        }

        return 1;
    }

    /*# get enabled
    *
    * @name camera.get_enabled
    * @param camera [type:url|number|nil] camera id
    * @return flag [type:boolean] true if the camera is enabled
    */
    static int RenderScriptCamera_GetEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext);
        lua_pushboolean(L, camera->m_Enabled);
        return 1;
    }

    /*# get projection matrix
    *
    * @name camera.get_projection
    * @param camera [type:url|number|nil] camera id
    * @return projection [type:matrix4] the projection matrix.
    */
    static int RenderScriptCamera_GetProjection(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext);
        dmScript::PushMatrix4(L, camera->m_Projection);
        return 1;
    }

    /*# get view matrix
    *
    * @name camera.get_view
    * @param camera [type:url|number|nil] camera id
    * @return view [type:matrix4] the view matrix.
    */
    static int RenderScriptCamera_GetView(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext);
        dmScript::PushMatrix4(L, camera->m_View);
        return 1;
    }

#define GET_CAMERA_DATA_PROPERTY_FN(param, lua_fn) \
    static int RenderScriptCamera_Get##param(lua_State* L) \
    { \
        DM_LUA_STACK_CHECK(L, 1); \
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext); \
        lua_fn(L, camera->m_Data.m_##param); \
        return 1; \
    }

#define SET_CAMERA_DATA_PROPERTY_FN(param, lua_fn) \
    static int RenderScriptCamera_Set##param(lua_State* L) \
    { \
        DM_LUA_STACK_CHECK(L, 0); \
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext); \
        camera->m_Data.m_##param = lua_fn(L, 2); \
        camera->m_Dirty = 1; \
        return 0; \
    }

    /*# get aspect ratio
    * Gets the effective aspect ratio of the camera. If auto aspect ratio is enabled,
    * returns the aspect ratio calculated from the current render target dimensions.
    * Otherwise returns the manually set aspect ratio.
    *
    * @name camera.get_aspect_ratio
    * @param camera [type:url|number|nil] camera id
    * @return aspect_ratio [type:number] the effective aspect ratio.
    */
    static int RenderScriptCamera_GetAspectRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext);
        float effective_aspect_ratio = GetRenderCameraEffectiveAspectRatio(g_RenderScriptCameraModule.m_RenderContext, camera->m_Handle);
        lua_pushnumber(L, effective_aspect_ratio);
        return 1;
    }

    /*# get far z
    *
    * @name camera.get_far_z
    * @param camera [type:url|number|nil] camera id
    * @return far_z [type:number] the far z.
    */
    GET_CAMERA_DATA_PROPERTY_FN(FarZ, lua_pushnumber);

    /*# get field of view
    *
    * @name camera.get_fov
    * @param camera [type:url|number|nil] camera id
    * @return fov [type:number] the field of view.
    */
    GET_CAMERA_DATA_PROPERTY_FN(Fov, lua_pushnumber);

    /*# get near z
    *
    * @name camera.get_near_z
    * @param camera [type:url|number|nil] camera id
    * @return near_z [type:number] the near z.
    */
    GET_CAMERA_DATA_PROPERTY_FN(NearZ, lua_pushnumber);

    /*# get orthographic zoom
    *
    * @name camera.get_orthographic_zoom
    * @param camera [type:url|number|nil] camera id
    * @return orthographic_zoom [type:number] the zoom level when the camera uses orthographic projection.
    */
    GET_CAMERA_DATA_PROPERTY_FN(OrthographicZoom, lua_pushnumber);

    /*# set aspect ratio
    * Sets the manual aspect ratio for the camera. This value is only used when
    * auto aspect ratio is disabled. To disable auto aspect ratio and use this
    * manual value, call camera.set_auto_aspect_ratio(camera, false).
    *
    * @name camera.set_aspect_ratio
    * @param camera [type:url|number|nil] camera id
    * @param aspect_ratio [type:number] the manual aspect ratio value.
    */
    SET_CAMERA_DATA_PROPERTY_FN(AspectRatio, lua_tonumber);

    /*# set far z
    *
    * @name camera.set_far_z
    * @param camera [type:url|number|nil] camera id
    * @param far_z [type:number] the far z.
    */
    SET_CAMERA_DATA_PROPERTY_FN(FarZ, lua_tonumber);

    /*# set field of view
    *
    * @name camera.set_fov
    * @param camera [type:url|number|nil] camera id
    * @param fov [type:number] the field of view.
    */
    SET_CAMERA_DATA_PROPERTY_FN(Fov, lua_tonumber);

    /*# set near z
    *
    * @name camera.set_near_z
    * @param camera [type:url|number|nil] camera id
    * @param near_z [type:number] the near z.
    */
    SET_CAMERA_DATA_PROPERTY_FN(NearZ, lua_tonumber);

    /*# set orthographic zoom
    *
    * @name camera.set_orthographic_zoom
    * @param camera [type:url|number|nil] camera id
    * @param orthographic_zoom [type:number] the zoom level when the camera uses orthographic projection.
    */
    SET_CAMERA_DATA_PROPERTY_FN(OrthographicZoom, lua_tonumber);

    /*# get orthographic zoom mode
    *
    * @name camera.get_orthographic_mode
    * @param camera [type:url|number|nil] camera id
    * @return mode [type:number] one of camera.ORTHO_MODE_FIXED, camera.ORTHO_MODE_AUTO_FIT or
    * camera.ORTHO_MODE_AUTO_COVER
    */
    GET_CAMERA_DATA_PROPERTY_FN(OrthographicMode, lua_pushnumber);

    // Helper for enum parameter validation used by macro setter
    static uint8_t LuaCheckOrthoZoomMode(lua_State* L, int index)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int mode = luaL_checkinteger(L, index);
        if (mode != ORTHO_MODE_FIXED &&
            mode != ORTHO_MODE_AUTO_FIT &&
            mode != ORTHO_MODE_AUTO_COVER)
        {
            return (uint8_t) DM_LUA_ERROR("Invalid orthographic zoom mode: %d", mode);
        }
        return (uint8_t) mode;
    }

    /*# set orthographic zoom mode
    *
    * @name camera.set_orthographic_mode
    * @param camera [type:url|number|nil] camera id
    * @param mode [type:number] camera.ORTHO_MODE_FIXED, camera.ORTHO_MODE_AUTO_FIT or camera.ORTHO_MODE_AUTO_COVER
    */
    SET_CAMERA_DATA_PROPERTY_FN(OrthographicMode, LuaCheckOrthoZoomMode);

    /*# get auto aspect ratio
    * Returns whether auto aspect ratio is enabled. When enabled, the camera automatically
    * calculates aspect ratio from render target dimensions. When disabled, uses the
    * manually set aspect ratio value.
    *
    * @name camera.get_auto_aspect_ratio
    * @param camera [type:url|number|nil] camera id
    * @return auto_aspect_ratio [type:boolean] true if auto aspect ratio is enabled
    */
    static int RenderScriptCamera_GetAutoAspectRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext);
        lua_pushboolean(L, camera->m_Data.m_AutoAspectRatio);
        return 1;
    }

    /*# set auto aspect ratio
    * Enables or disables automatic aspect ratio calculation. When enabled (true),
    * the camera automatically calculates aspect ratio from render target dimensions.
    * When disabled (false), uses the manually set aspect ratio value.
    *
    * @name camera.set_auto_aspect_ratio
    * @param camera [type:url|number|nil] camera id
    * @param auto_aspect_ratio [type:boolean] true to enable auto aspect ratio
    */
    static int RenderScriptCamera_SetAutoAspectRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext);
        camera->m_Data.m_AutoAspectRatio = lua_toboolean(L, 2) ? 1 : 0;
        camera->m_Dirty = 1;
        return 0;
    }

#undef GET_CAMERA_DATA_PROPERTY_FN
#undef SET_CAMERA_DATA_PROPERTY_FN

    static const luaL_reg RenderScriptCamera_Methods[] =
    {
        {"get_cameras",             RenderScriptCamera_GetCameras},

        // READ-ONLY
        {"get_projection",          RenderScriptCamera_GetProjection},
        {"get_view",                RenderScriptCamera_GetView},
        {"get_enabled",             RenderScriptCamera_GetEnabled},

        // CONVERSIONS
        {"screen_xy_to_world",      RenderScriptCamera_ScreenXYToWorld},
        {"screen_to_world",         RenderScriptCamera_ScreenToWorld},
        {"world_to_screen",         RenderScriptCamera_WorldToScreen},

        // READ-WRITE
        {"get_aspect_ratio",        RenderScriptCamera_GetAspectRatio},
        {"set_aspect_ratio",        RenderScriptCamera_SetAspectRatio},
        {"get_fov",                 RenderScriptCamera_GetFov},
        {"set_fov",                 RenderScriptCamera_SetFov},
        {"get_near_z",              RenderScriptCamera_GetNearZ},
        {"set_near_z",              RenderScriptCamera_SetNearZ},
        {"get_far_z",               RenderScriptCamera_GetFarZ},
        {"set_far_z",               RenderScriptCamera_SetFarZ},
        {"get_orthographic_zoom",   RenderScriptCamera_GetOrthographicZoom},
        {"set_orthographic_zoom",   RenderScriptCamera_SetOrthographicZoom},
        {"get_auto_aspect_ratio",   RenderScriptCamera_GetAutoAspectRatio},
        {"set_auto_aspect_ratio",   RenderScriptCamera_SetAutoAspectRatio},
        {"get_orthographic_mode",   RenderScriptCamera_GetOrthographicMode},
        {"set_orthographic_mode",   RenderScriptCamera_SetOrthographicMode},
        {0, 0}
    };

    void InitializeRenderScriptCameraContext(HRenderContext render_context, dmScript::HContext script_context)
    {
        lua_State* L = dmScript::GetLuaState(script_context);
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, RENDER_SCRIPT_CAMERA_LIB_NAME, RenderScriptCamera_Methods);

        // Add constants: camera.ORTHO_MODE_FIXED/AUTO_FIT/AUTO_COVER
        #define SETCONSTANT(name, val) \
            lua_pushnumber(L, (lua_Number) (val)); \
            lua_setfield(L, -2, #name);

        SETCONSTANT(ORTHO_MODE_FIXED,      dmRender::ORTHO_MODE_FIXED);
        SETCONSTANT(ORTHO_MODE_AUTO_FIT,   dmRender::ORTHO_MODE_AUTO_FIT);
        SETCONSTANT(ORTHO_MODE_AUTO_COVER, dmRender::ORTHO_MODE_AUTO_COVER);

        #undef SETCONSTANT

        lua_pop(L, 1);

        assert(g_RenderScriptCameraModule.m_RenderContext == 0x0);
        g_RenderScriptCameraModule.m_RenderContext = render_context;
    }

    void FinalizeRenderScriptCameraContext(HRenderContext render_context)
    {
        g_RenderScriptCameraModule.m_RenderContext = 0x0;
    }
}
