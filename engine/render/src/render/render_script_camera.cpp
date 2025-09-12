// Copyright 2020-2025 The Defold Foundation
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
     * @name camera.ORTHO_ZOOM_MODE_FIXED
     * @constant
     */

    /*# auto-fit orthographic zoom mode
     * Computes zoom so the original display area (game.project width/height) fits inside the window
     * while preserving aspect ratio. Equivalent to using min(window_width/width, window_height/height).
     *
     * @name camera.ORTHO_ZOOM_MODE_AUTO_FIT
     * @constant
     */

    /*# auto-cover orthographic zoom mode
     * Computes zoom so the original display area covers the entire window while preserving aspect ratio.
     * Equivalent to using max(window_width/width, window_height/height).
     *
     * @name camera.ORTHO_ZOOM_MODE_AUTO_COVER
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
    * @name camera.get_orthographic_zoom_mode
    * @param camera [type:url|number|nil] camera id
    * @return mode [type:number] one of camera.ORTHO_ZOOM_MODE_FIXED, camera.ORTHO_ZOOM_MODE_AUTO_FIT or
    * camera.ORTHO_ZOOM_MODE_AUTO_COVER
    */
    GET_CAMERA_DATA_PROPERTY_FN(OrthographicZoomMode, lua_pushnumber);

    // Helper for enum parameter validation used by macro setter
    static uint8_t LuaCheckOrthoZoomMode(lua_State* L, int index)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int mode = luaL_checkinteger(L, index);
        if (mode != ORTHO_ZOOM_MODE_FIXED &&
            mode != ORTHO_ZOOM_MODE_AUTO_FIT &&
            mode != ORTHO_ZOOM_MODE_AUTO_COVER)
        {
            return (uint8_t) DM_LUA_ERROR("Invalid orthographic zoom mode: %d", mode);
        }
        return (uint8_t) mode;
    }

    /*# set orthographic zoom mode
    *
    * @name camera.set_orthographic_zoom_mode
    * @param camera [type:url|number|nil] camera id
    * @param mode [type:number] camera.ORTHO_ZOOM_MODE_FIXED, _AUTO_FIT or _AUTO_COVER
    */
    SET_CAMERA_DATA_PROPERTY_FN(OrthographicZoomMode, LuaCheckOrthoZoomMode);

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
        {"get_orthographic_zoom_mode",   RenderScriptCamera_GetOrthographicZoomMode},
        {"set_orthographic_zoom_mode",   RenderScriptCamera_SetOrthographicZoomMode},
        {0, 0}
    };

    void InitializeRenderScriptCameraContext(HRenderContext render_context, dmScript::HContext script_context)
    {
        lua_State* L = dmScript::GetLuaState(script_context);
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, RENDER_SCRIPT_CAMERA_LIB_NAME, RenderScriptCamera_Methods);

        // Add constants: camera.ORTHO_ZOOM_MODE_FIXED/AUTO_FIT/AUTO_COVER
        #define SETCONSTANT(name, val) \
            lua_pushnumber(L, (lua_Number) (val)); \
            lua_setfield(L, -2, #name);

        SETCONSTANT(ORTHO_ZOOM_MODE_FIXED,      dmRender::ORTHO_ZOOM_MODE_FIXED);
        SETCONSTANT(ORTHO_ZOOM_MODE_AUTO_FIT,   dmRender::ORTHO_ZOOM_MODE_AUTO_FIT);
        SETCONSTANT(ORTHO_ZOOM_MODE_AUTO_COVER, dmRender::ORTHO_ZOOM_MODE_AUTO_COVER);

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
