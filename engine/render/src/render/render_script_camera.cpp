// Copyright 2020-2024 The Defold Foundation
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
     */
    #define RENDER_SCRIPT_CAMERA_MAIN
    #define RENDER_SCRIPT_CAMERA_LIB_NAME           "camera"
    #define RENDER_SCRIPT_CAMERA_MAIN_NAME          "CameraMain"
    #define RENDER_SCRIPT_CAMERA_MAIN_PROPERTY_NAME "MAIN"

    struct RenderScriptCameraModule
    {
        dmRender::HRenderContext m_RenderContext;
        dmMessage::URL           m_MainCameraURL;
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
                luaL_error(L, "Invalid handle.");
                return 0;
            }
        }
        else
        {
            dmMessage::URL url;
            if (dmScript::ResolveURL(L, index, &url, 0) != dmMessage::RESULT_OK)
            {
                luaL_error(L, "Could not resolve URL.");
                return 0;
            }

            camera = GetRenderCameraByUrl(g_RenderScriptCameraModule.m_RenderContext, url);
            if (camera == 0x0)
            {
                char buffer[256];
                dmScript::UrlToString(&url, buffer, sizeof(buffer));
                luaL_error(L, "Camera '%s' not found.", buffer);
                return 0;
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

    /*# get camera info
    * Get the info for a specific camera by URL. The result is a table with the following fields:
    *
    * `url`
    * : [type:url] the URL of the camera.
    *
    * `projection`
    * : [type:vmath.matrix4] the projection matrix.
    *
    * `view`
    * : [type:vmath.matrix4] the view matrix.
    *
    * `viewport`
    * : [type:vmath.vector4] the viewport.
    *
    * `handle`
    * : [type:number] the handle of the camera.
    *
    * `main_camera`
    * : [type:boolean] true if this is the main camera.
    *
    * `fov`
    * : [type:number] the field of view.
    *
    * `aspect_ratio`
    * : [type:number] the aspect ratio.
    *
    * `near_z`
    * : [type:number] the near z.
    *
    * `far_z`
    * : [type:number] the far z.
    *
    * `orthographic_projection`
    * : [type:boolean] true if the camera is using an orthographic projection.
    *
    * `auto_aspect_ratio`
    * : [type:boolean] true if the camera is using an automatic aspect ratio.
    *
    * @name camera.get_cameras
    * @return cameras [type:table] a table with all camera URLs
    *
    * @examples
    * ```lua
    * local info = camera.get_info("main:/my_go#camera")
    * render.set_camera(info.handle)
    * ```
    */
    static int RenderScriptCamera_GetInfo(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        RenderCamera* camera = CheckRenderCamera(L, 1, g_RenderScriptCameraModule.m_RenderContext);

        lua_newtable(L);
        lua_pushstring(L, "url");
        dmScript::PushURL(L, camera->m_URL);
        lua_settable(L, -3);

        lua_pushstring(L, "projection");
        dmScript::PushMatrix4(L, camera->m_Projection);
        lua_settable(L, -3);

        lua_pushstring(L, "view");
        dmScript::PushMatrix4(L, camera->m_View);
        lua_settable(L, -3);

        lua_pushstring(L, "handle");
        lua_pushnumber(L, camera->m_Handle);
        lua_settable(L, -3);

        lua_pushstring(L, "viewport");
        dmScript::PushVector4(L, camera->m_Data.m_Viewport);
        lua_settable(L, -3);

        lua_pushstring(L, "main_camera");
        lua_pushboolean(L, camera->m_IsMainCamera);
        lua_settable(L, -3);

    #define PUSH_NUMBER(name, param) \
        lua_pushstring(L, name); \
        lua_pushnumber(L, camera->m_Data.param); \
        lua_settable(L, -3);

        PUSH_NUMBER("fov",          m_Fov);
        PUSH_NUMBER("aspect_ratio", m_AspectRatio);
        PUSH_NUMBER("near_z",       m_NearZ);
        PUSH_NUMBER("far_z",        m_FarZ);
    #undef PUSH_NUMBER

    #define PUSH_BOOL(name, param) \
        lua_pushstring(L, name); \
        lua_pushboolean(L, camera->m_Data.param); \
        lua_settable(L, -3);

        PUSH_BOOL("orthographic_projection", m_OrthographicProjection);
        PUSH_BOOL("auto_aspect_ratio",       m_AutoAspectRatio);
    #undef PUSH_BOOL

        return 1;
    }

    // This is called when getting a property on the camera module.
    // If the 'MAIN' property is called, we return the URL to the current main camera.
    // Otherwise, we just return the tble entry.
    static int RenderScriptCamera_index(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, RENDER_SCRIPT_CAMERA_MAIN_PROPERTY_NAME) == 0)
        {
            dmScript::PushURL(L, g_RenderScriptCameraModule.m_MainCameraURL);
        }
        else
        {
            lua_gettable(L, -2);
        }
        return 1;
    }

    // This is called when setting a property on the camera module.
    // A user can change the main camera property by setting the MAIN property
    // to a different camera URL. In that case we check that the URL is an actual
    // valid camera.
    static int RenderScriptCamera_newindex(lua_State* L)
    {
        // Stack:
        // 1: table
        // 2: key
        // 3: value
        DM_LUA_STACK_CHECK(L, -3); // must return 0, so we need to pop all three items
        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, RENDER_SCRIPT_CAMERA_MAIN_PROPERTY_NAME) == 0)
        {
            RenderCamera* camera = CheckRenderCamera(L, 3, g_RenderScriptCameraModule.m_RenderContext);
            RenderScriptCameraSetMainCamera(camera->m_URL);
        }
        lua_rawset(L, -3); // -2 (set table[key] = value)
        lua_pop(L, 1);     // -1 (pop table)
        return 0;
    }

    static const luaL_reg RenderScriptCameraMain_Methods[] =
    {
        {0, 0}
    };

    static const luaL_reg RenderScriptCameraMain_Meta[] =
    {
        {"__newindex",  RenderScriptCamera_newindex},
        {"__index", RenderScriptCamera_index},
        {0, 0}
    };

    static const luaL_reg RenderScriptCamera_Methods[] =
    {
        {"get_cameras", RenderScriptCamera_GetCameras},
        {"get_info",    RenderScriptCamera_GetInfo},
        {0, 0}
    };

    void RenderScriptCameraSetMainCamera(const dmMessage::URL& camera_url)
    {
        g_RenderScriptCameraModule.m_MainCameraURL = camera_url;
    }

    /*# Main camera URL
     * The URL of the main camera. If set, this changes what is considered the current main camera.
     *
     * @name camera.MAIN
     * @variable
     */

    void InitializeRenderScriptCameraContext(HRenderContext render_context, dmScript::HContext script_context)
    {
        lua_State* L = dmScript::GetLuaState(script_context);
        DM_LUA_STACK_CHECK(L, 0);

        dmScript::RegisterUserType(L, RENDER_SCRIPT_CAMERA_MAIN_NAME, RenderScriptCameraMain_Methods, RenderScriptCameraMain_Meta);

        luaL_register(L, RENDER_SCRIPT_CAMERA_LIB_NAME, RenderScriptCamera_Methods);

        luaL_getmetatable(L, RENDER_SCRIPT_CAMERA_MAIN_NAME);
        lua_setmetatable(L, -2);

        lua_pop(L, 1);

        assert(g_RenderScriptCameraModule.m_RenderContext == 0x0);
        g_RenderScriptCameraModule.m_RenderContext = render_context;
    }

    void FinalizeRenderScriptCameraContext(HRenderContext render_context)
    {
        g_RenderScriptCameraModule.m_RenderContext = 0x0;
    }
}
