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

#include "comp_camera.h"

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dmsdk/dlib/vmath.h>
#include <dlib/profile.h>

#include <render/render.h>

#include "../resources/res_camera.h"
#include <gamesys/gamesys_ddf.h>
#include "../gamesys_private.h"
#include "comp_private.h"

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Camera, 0, FrameReset, "# components", &rmtp_Components);

namespace dmGameSystem
{
    using namespace dmVMath;

    const uint32_t MAX_COUNT = 64;
    const uint8_t MAX_STACK_COUNT = 8;

    struct CameraWorld;

    struct CameraComponent
    {
        dmGameObject::HInstance m_Instance;
        dmRender::HRenderCamera m_RenderCamera;
        CameraWorld*            m_World;
        dmVMath::Matrix4        m_View;
        dmVMath::Matrix4        m_Projection;
        float                   m_AspectRatio;
        float                   m_Fov;
        float                   m_NearZ;
        float                   m_FarZ;
        float                   m_OrthographicZoom;
        uint16_t                m_ComponentIndex;
        uint8_t                 m_AutoAspectRatio        : 1;
        uint8_t                 m_AddedToUpdate          : 1;
        uint8_t                 m_OrthographicProjection : 1;
    };

    struct CameraWorld
    {
        dmArray<CameraComponent> m_Cameras;
        dmArray<CameraComponent*> m_FocusStack;
    };

    static const dmhash_t CAMERA_PROP_FOV = dmHashString64("fov");
    static const dmhash_t CAMERA_PROP_NEAR_Z = dmHashString64("near_z");
    static const dmhash_t CAMERA_PROP_FAR_Z = dmHashString64("far_z");
    static const dmhash_t CAMERA_PROP_ORTHOGRAPHIC_ZOOM = dmHashString64("orthographic_zoom");
    static const dmhash_t CAMERA_PROP_PROJECTION = dmHashString64("projection");
    static const dmhash_t CAMERA_PROP_VIEW = dmHashString64("view");
    static const dmhash_t CAMERA_PROP_ASPECT_RATIO = dmHashString64("aspect_ratio");


    void CompCameraUpdateViewProjection(CameraComponent* camera, dmRender::RenderContext* render_context)
    {
        float width = (float)dmGraphics::GetWindowWidth(dmRender::GetGraphicsContext(render_context));
        float height = (float)dmGraphics::GetWindowHeight(dmRender::GetGraphicsContext(render_context));

        float aspect_ratio = camera->m_AspectRatio;
        if (camera->m_AutoAspectRatio)
        {
            aspect_ratio = width / height;
        }

        dmVMath::Point3 pos = dmGameObject::GetWorldPosition(camera->m_Instance);
        if (camera->m_OrthographicProjection)
        {
            float display_scale = dmGraphics::GetDisplayScaleFactor(dmRender::GetGraphicsContext(render_context));
            float zoom = camera->m_OrthographicZoom;
            float zoomed_width = width / display_scale / zoom;
            float zoomed_height = height / display_scale / zoom;

            float left = -zoomed_width / 2;
            float right = zoomed_width / 2;
            float bottom = -zoomed_height / 2;
            float top = zoomed_height / 2;
            camera->m_Projection = Matrix4::orthographic(left, right, bottom, top, camera->m_NearZ, camera->m_FarZ);
        }
        else
        {
            camera->m_Projection = Matrix4::perspective(camera->m_Fov, aspect_ratio, camera->m_NearZ, camera->m_FarZ);
        }

        dmVMath::Quat rot = dmGameObject::GetWorldRotation(camera->m_Instance);
        Point3 look_at = pos + dmVMath::Rotate(rot, dmVMath::Vector3(0.0f, 0.0f, -1.0f));
        Vector3 up = dmVMath::Rotate(rot, dmVMath::Vector3(0.0f, 1.0f, 0.0f));
        camera->m_View = Matrix4::lookAt(pos, look_at, up);
    }

    dmGameObject::CreateResult CompCameraNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        CameraWorld* cam_world = new CameraWorld();
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, MAX_COUNT);
        cam_world->m_Cameras.SetCapacity(comp_count);
        cam_world->m_FocusStack.SetCapacity(MAX_STACK_COUNT);
        *params.m_World = cam_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCameraDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        delete (CameraWorld*)params.m_World;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCameraCreate(const dmGameObject::ComponentCreateParams& params)
    {
        CameraWorld* w = (CameraWorld*)params.m_World;
        if (w->m_Cameras.Full())
        {
            ShowFullBufferError("Camera", MAX_COUNT);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        dmGameSystem::CameraResource* cam_resource = (CameraResource*)params.m_Resource;
        CameraComponent camera;
        camera.m_Instance = params.m_Instance;
        camera.m_World = w;
        camera.m_AspectRatio = cam_resource->m_DDF->m_AspectRatio;
        camera.m_Fov = cam_resource->m_DDF->m_Fov;
        camera.m_NearZ = cam_resource->m_DDF->m_NearZ;
        camera.m_FarZ = cam_resource->m_DDF->m_FarZ;
        camera.m_AutoAspectRatio = cam_resource->m_DDF->m_AutoAspectRatio != 0;
        camera.m_OrthographicProjection = cam_resource->m_DDF->m_OrthographicProjection != 0;
        camera.m_OrthographicZoom = cam_resource->m_DDF->m_OrthographicZoom;
        camera.m_AddedToUpdate = 0;
        camera.m_ComponentIndex = params.m_ComponentIndex;
        dmRender::RenderContext* render_context = (dmRender::RenderContext*)params.m_Context;
        CompCameraUpdateViewProjection(&camera, render_context);
        w->m_Cameras.Push(camera);
        *params.m_UserData = (uintptr_t)&w->m_Cameras[w->m_Cameras.Size() - 1];

        dmMessage::URL url_id;
        url_id.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(camera.m_Instance));
        url_id.m_Path   = dmGameObject::GetIdentifier(camera.m_Instance);
        dmGameObject::GetComponentId(camera.m_Instance, camera.m_ComponentIndex, &url_id.m_Fragment);

        camera.m_RenderCamera = dmRender::NewRenderCamera(render_context, url_id);
        SetRenderCameraData(render_context, camera.m_RenderCamera, camera.m_View, camera.m_Projection);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCameraDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        CameraWorld* w = (CameraWorld*)params.m_World;
        CameraComponent* camera = (CameraComponent*)*params.m_UserData;
        bool found = false;
        for (uint8_t i = 0; i < w->m_FocusStack.Size(); ++i)
        {
            if (w->m_FocusStack[i] == camera)
            {
                found = true;
            }
            if (found && i < w->m_FocusStack.Size() - 1)
            {
                w->m_FocusStack[i] = w->m_FocusStack[i + 1];
            }
        }
        if (found)
        {
            w->m_FocusStack.Pop();
        }
        for (uint8_t i = 0; i < w->m_Cameras.Size(); ++i)
        {
            if (w->m_Cameras[i].m_Instance == params.m_Instance)
            {
                w->m_Cameras.EraseSwap(i);
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        dmLogError("Destroyed camera could not be found.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompCameraAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        CameraComponent* camera = (CameraComponent*)*params.m_UserData;
        camera->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCameraUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        CameraWorld* camera_world = (CameraWorld*) params.m_World;
        DM_PROPERTY_ADD_U32(rmtp_Camera, camera_world->m_Cameras.Size());

        uint32_t stack_size = camera_world->m_FocusStack.Size();

        if (stack_size > 0)
        {
            dmRender::RenderContext* render_context = (dmRender::RenderContext*) params.m_Context;
            dmhash_t message_id                     = dmGameSystemDDF::SetViewProjection::m_DDFDescriptor->m_NameHash;
            bool any_active_camera                  = false;

            for (int i = 0; i < stack_size; ++i)
            {
                CameraComponent* camera = camera_world->m_FocusStack[stack_size - 1 - i];
                if (camera->m_AddedToUpdate)
                {
                    CompCameraUpdateViewProjection(camera, render_context);

                    dmGameSystemDDF::SetViewProjection set_view_projection;
                    set_view_projection.m_View       = camera->m_View;
                    set_view_projection.m_Projection = camera->m_Projection;

                    dmGameObject::Result go_result = dmGameObject::GetComponentId(camera->m_Instance, camera->m_ComponentIndex, &set_view_projection.m_Id);
                    if (go_result != dmGameObject::RESULT_OK)
                    {
                        dmLogError("Could not send set_view_projection because of incomplete component.");
                        return dmGameObject::UPDATE_RESULT_OK;
                    }

                    dmMessage::URL receiver;
                    dmMessage::ResetURL(&receiver);
                    dmMessage::Result result = dmMessage::GetSocket(dmRender::RENDER_SOCKET_NAME, &receiver.m_Socket);
                    if (result != dmMessage::RESULT_OK)
                    {
                        dmLogError("The socket '%s' could not be found.", dmRender::RENDER_SOCKET_NAME);
                        return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                    }

                    dmMessage::URL sender;
                    sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(camera->m_Instance));
                    sender.m_Path = dmGameObject::GetIdentifier(camera->m_Instance);
                    dmGameObject::GetComponentId(camera->m_Instance, camera->m_ComponentIndex, &sender.m_Fragment);

                    dmMessage::Post(&sender, &receiver, message_id, 0, (uintptr_t)dmGameSystemDDF::SetViewProjection::m_DDFDescriptor, &set_view_projection, sizeof(dmGameSystemDDF::SetViewProjection), 0);

                    SetRenderCameraData(render_context, camera->m_RenderCamera, camera->m_View, camera->m_Projection);

                    // Note: We only set the view/projection on the first active camera in the focus stack
                    if (!any_active_camera)
                    {
                        // JG: What does this TODO mean here?
                        // TODO: Remove this once render scripts are implemented everywhere
                        dmRender::SetProjectionMatrix(render_context, camera->m_Projection);
                        dmRender::SetViewMatrix(render_context, camera->m_View);
                        any_active_camera = true;
                    }
                }
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCameraOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        CameraComponent* camera = (CameraComponent*)*params.m_UserData;
        if ((dmDDF::Descriptor*)params.m_Message->m_Descriptor == dmGamesysDDF::SetCamera::m_DDFDescriptor)
        {
            dmGamesysDDF::SetCamera* ddf = (dmGamesysDDF::SetCamera*)params.m_Message->m_Data;
            camera->m_AspectRatio = ddf->m_AspectRatio;
            camera->m_Fov = ddf->m_Fov;
            camera->m_NearZ = ddf->m_NearZ;
            camera->m_FarZ = ddf->m_FarZ;
            camera->m_OrthographicProjection = ddf->m_OrthographicProjection;
            camera->m_OrthographicZoom = ddf->m_OrthographicZoom;
        }
        else if ((dmDDF::Descriptor*)params.m_Message->m_Descriptor == dmGamesysDDF::AcquireCameraFocus::m_DDFDescriptor)
        {
            bool found = false;
            for (uint32_t i = 0; i < camera->m_World->m_FocusStack.Size(); ++i)
            {
                if (camera->m_World->m_FocusStack[i] == camera)
                {
                    found = true;
                }
                if (found && i < camera->m_World->m_FocusStack.Size() - 1)
                {
                    camera->m_World->m_FocusStack[i] = camera->m_World->m_FocusStack[i + 1];
                }
            }
            if (found)
            {
                camera->m_World->m_FocusStack.Pop();
            }
            if (!camera->m_World->m_FocusStack.Full())
            {
                camera->m_World->m_FocusStack.Push(camera);
            }
            else
            {
                LogMessageError(params.m_Message, "Could not acquire camera focus since the buffer is full (%d).", camera->m_World->m_FocusStack.Size());
            }
        }
        else if ((dmDDF::Descriptor*)params.m_Message->m_Descriptor == dmGamesysDDF::ReleaseCameraFocus::m_DDFDescriptor)
        {
            bool found = false;
            for (uint32_t i = 0; i < camera->m_World->m_FocusStack.Size(); ++i)
            {
                if (camera->m_World->m_FocusStack[i] == camera)
                {
                    found = true;
                }
                if (found && i < camera->m_World->m_FocusStack.Size() - 1)
                {
                    camera->m_World->m_FocusStack[i] = camera->m_World->m_FocusStack[i + 1];
                }
            }
            if (found)
            {
                camera->m_World->m_FocusStack.Pop();
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompCameraOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        CameraResource* cam_resource = (CameraResource*)params.m_Resource;
        CameraComponent* camera = (CameraComponent*)*params.m_UserData;
        camera->m_AspectRatio = cam_resource->m_DDF->m_AspectRatio;
        camera->m_Fov = cam_resource->m_DDF->m_Fov;
        camera->m_NearZ = cam_resource->m_DDF->m_NearZ;
        camera->m_FarZ = cam_resource->m_DDF->m_FarZ;
        camera->m_AutoAspectRatio = cam_resource->m_DDF->m_AutoAspectRatio != 0;
        camera->m_OrthographicProjection = cam_resource->m_DDF->m_OrthographicProjection != 0;
        camera->m_OrthographicZoom = cam_resource->m_DDF->m_OrthographicZoom;
        dmRender::RenderContext* render_context = (dmRender::RenderContext*)params.m_Context;
        CompCameraUpdateViewProjection(camera, render_context);
    }

    dmGameObject::PropertyResult CompCameraGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        CameraComponent* component = (CameraComponent*)*params.m_UserData;
        dmhash_t get_property = params.m_PropertyId;

        if (CAMERA_PROP_FOV == get_property)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_Fov);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_NEAR_Z == get_property)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_NearZ);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_FAR_Z == get_property)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_FarZ);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_ORTHOGRAPHIC_ZOOM == get_property)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_OrthographicZoom);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_PROJECTION == get_property)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_Projection);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_VIEW == get_property)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_View);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_ASPECT_RATIO == get_property)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_AspectRatio);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult CompCameraSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        CameraComponent* component = (CameraComponent*)*params.m_UserData;
        dmhash_t set_property = params.m_PropertyId;

        if (CAMERA_PROP_FOV == set_property)
        {
            component->m_Fov = params.m_Value.m_Number;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_NEAR_Z == set_property)
        {
            component->m_NearZ = params.m_Value.m_Number;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_FAR_Z == set_property)
        {
            component->m_FarZ = params.m_Value.m_Number;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_ORTHOGRAPHIC_ZOOM == set_property)
        {
            component->m_OrthographicZoom = params.m_Value.m_Number;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (CAMERA_PROP_ASPECT_RATIO == set_property)
        {
            component->m_AspectRatio = params.m_Value.m_Number;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if ((CAMERA_PROP_PROJECTION == set_property)
            || (CAMERA_PROP_VIEW == set_property)
            || (CAMERA_PROP_ASPECT_RATIO == set_property))
        {
            return dmGameObject::PROPERTY_RESULT_READ_ONLY;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }
}
