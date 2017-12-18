#include "comp_camera.h"

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <render/render.h>

#include "../resources/res_camera.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    const uint8_t MAX_COUNT = 64;
    const uint8_t MAX_STACK_COUNT = 8;

    struct CameraWorld;

    struct CameraComponent
    {
        dmGameObject::HInstance m_Instance;
        CameraWorld* m_World;
        float m_AspectRatio;
        float m_Fov;
        float m_NearZ;
        float m_FarZ;
        uint32_t m_AutoAspectRatio : 1;
        uint32_t m_AddedToUpdate : 1;
    };

    struct CameraWorld
    {
        dmArray<CameraComponent> m_Cameras;
        dmArray<CameraComponent*> m_FocusStack;
    };

    dmGameObject::CreateResult CompCameraNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        CameraWorld* cam_world = new CameraWorld();
        cam_world->m_Cameras.SetCapacity(MAX_COUNT);
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
        if (!w->m_Cameras.Full())
        {
            dmGameSystem::CameraResource* cam_resource = (CameraResource*)params.m_Resource;
            CameraComponent camera;
            camera.m_Instance = params.m_Instance;
            camera.m_World = w;
            camera.m_AspectRatio = cam_resource->m_DDF->m_AspectRatio;
            camera.m_Fov = cam_resource->m_DDF->m_Fov;
            camera.m_NearZ = cam_resource->m_DDF->m_NearZ;
            camera.m_FarZ = cam_resource->m_DDF->m_FarZ;
            camera.m_AutoAspectRatio = cam_resource->m_DDF->m_AutoAspectRatio != 0;
            camera.m_AddedToUpdate = 0;
            w->m_Cameras.Push(camera);
            *params.m_UserData = (uintptr_t)&w->m_Cameras[w->m_Cameras.Size() - 1];
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Camera buffer is full (%d), component disregarded.", MAX_COUNT);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
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
        dmLogError("Destroyed camera could not be found, something is fishy.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompCameraAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        CameraComponent* camera = (CameraComponent*)*params.m_UserData;
        camera->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCameraUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        CameraWorld* w = (CameraWorld*)params.m_World;
        CameraComponent* camera = 0x0;
        if (w->m_FocusStack.Size() > 0)
        {
            camera = w->m_FocusStack[w->m_FocusStack.Size() - 1];
        }
        if (camera != 0x0 && camera->m_AddedToUpdate)
        {
            dmRender::RenderContext* render_context = (dmRender::RenderContext*)params.m_Context;

            float aspect_ratio = camera->m_AspectRatio;
            if (camera->m_AutoAspectRatio)
            {
                float width = (float)dmGraphics::GetWindowWidth(dmRender::GetGraphicsContext(render_context));
                float height = (float)dmGraphics::GetWindowHeight(dmRender::GetGraphicsContext(render_context));
                aspect_ratio = width / height;
            }
            Vectormath::Aos::Matrix4 projection = Matrix4::perspective(camera->m_Fov, aspect_ratio, camera->m_NearZ, camera->m_FarZ);

            Vectormath::Aos::Point3 pos = dmGameObject::GetWorldPosition(camera->m_Instance);
            Vectormath::Aos::Quat rot = dmGameObject::GetWorldRotation(camera->m_Instance);
            Point3 look_at = pos + Vectormath::Aos::rotate(rot, Vectormath::Aos::Vector3(0.0f, 0.0f, -1.0f));
            Vector3 up = Vectormath::Aos::rotate(rot, Vectormath::Aos::Vector3(0.0f, 1.0f, 0.0f));
            Vectormath::Aos::Matrix4 view = Matrix4::lookAt(pos, look_at, up);

            // Send the matrices to the render script

            dmhash_t message_id = dmGameSystemDDF::SetViewProjection::m_DDFDescriptor->m_NameHash;

            dmGameSystemDDF::SetViewProjection set_view_projection;
            set_view_projection.m_Id = dmHashString64("game");
            set_view_projection.m_View = view;
            set_view_projection.m_Projection = projection;

            dmMessage::URL receiver;
            dmMessage::ResetURL(receiver);
            dmMessage::Result result = dmMessage::GetSocket(dmRender::RENDER_SOCKET_NAME, &receiver.m_Socket);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("The socket '%s' could not be found.", dmRender::RENDER_SOCKET_NAME);
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }

            dmMessage::Post(0x0, &receiver, message_id, 0, (uintptr_t)dmGameSystemDDF::SetViewProjection::m_DDFDescriptor, &set_view_projection, sizeof(dmGameSystemDDF::SetViewProjection), 0);

            // Set matrices immediately
            // TODO: Remove this once render scripts are implemented everywhere
            dmRender::SetProjectionMatrix(render_context, projection);
            dmRender::SetViewMatrix(render_context, view);
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
    }
}
