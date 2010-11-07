#include "comp_camera.h"

#include <dlib/array.h>
#include <dlib/circular_array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <render/render.h>

#include "../resources/res_camera.h"
#include "gamesys_ddf.h"

namespace dmGameSystem
{
    const uint8_t MAX_COUNT = 64;
    const uint8_t MAX_STACK_COUNT = 8;

    struct CameraWorld;

    struct Camera
    {
        dmGameObject::HInstance m_Instance;
        dmGameSystem::CameraProperties* m_Properties;
        CameraWorld* m_World;
        float m_AspectRatio;
        float m_FOV;
        float m_NearZ;
        float m_FarZ;
    };

    struct CameraWorld
    {
        dmArray<Camera> m_Cameras;
        dmCircularArray<Camera*> m_FocusStack;
    };

    dmGameObject::CreateResult CompCameraNewWorld(void* context, void** world)
    {
        CameraWorld* cam_world = new CameraWorld();
        cam_world->m_Cameras.SetCapacity(MAX_COUNT);
        cam_world->m_FocusStack.SetCapacity(MAX_STACK_COUNT);
        *world = cam_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCameraDeleteWorld(void* context, void* world)
    {
        delete (CameraWorld*)world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCameraCreate(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        CameraWorld* w = (CameraWorld*)world;
        if (w->m_Cameras.Size() < MAX_COUNT)
        {
            Camera camera;
            camera.m_Instance = instance;
            camera.m_Properties = (CameraProperties*)resource;
            camera.m_World = w;
            camera.m_AspectRatio = camera.m_Properties->m_DDF->m_AspectRatio;
            camera.m_FOV = camera.m_Properties->m_DDF->m_FOV;
            camera.m_NearZ = camera.m_Properties->m_DDF->m_NearZ;
            camera.m_FarZ = camera.m_Properties->m_DDF->m_FarZ;
            w->m_Cameras.Push(camera);
            *user_data = (uintptr_t)&w->m_Cameras[w->m_Cameras.Size() - 1];
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Camera buffer is full (%d), component disregarded.", MAX_COUNT);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompCameraDestroy(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        CameraWorld* w = (CameraWorld*)world;
        Camera* camera = (Camera*)*user_data;
        for (uint8_t i = 0; i < w->m_FocusStack.Size(); ++i)
        {
            if (w->m_FocusStack[i] == camera)
            {
                w->m_FocusStack[i] = 0x0;
                break;
            }
        }
        for (uint8_t i = 0; i < w->m_Cameras.Size(); ++i)
        {
            if (w->m_Cameras[i].m_Instance == instance)
            {
                w->m_Cameras.EraseSwap(i);
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        dmLogError("Destroyed camera could not be found, something is fishy.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::UpdateResult CompCameraUpdate(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* world,
            void* context)
    {
        CameraWorld* w = (CameraWorld*)world;
        Camera* camera = 0x0;
        while (w->m_FocusStack.Size() > 0)
        {
            Camera* top = w->m_FocusStack.Top();
            if (top != 0x0)
            {
                camera = top;
                break;
            }
            else
                w->m_FocusStack.Pop();
        }
        if (camera != 0x0)
        {
            dmRender::RenderContext* render_context = (dmRender::RenderContext*)context;

            Vectormath::Aos::Matrix4 projection = Matrix4::perspective(camera->m_FOV, camera->m_AspectRatio, camera->m_NearZ, camera->m_FarZ);

            Vectormath::Aos::Point3 pos = dmGameObject::GetWorldPosition(camera->m_Instance);
            Vectormath::Aos::Quat rot = dmGameObject::GetWorldRotation(camera->m_Instance);
            Point3 look_at = pos + Vectormath::Aos::rotate(rot, Vectormath::Aos::Vector3(0.0f, 0.0f, -1.0f));
            Vector3 up = Vectormath::Aos::rotate(rot, Vectormath::Aos::Vector3(0.0f, 1.0f, 0.0f));
            Vectormath::Aos::Matrix4 view = Matrix4::lookAt(pos, look_at, up);

            // Send the matrices to the render script
            char buf[sizeof(dmGameObject::InstanceMessageData) + sizeof(dmGameSystemDDF::SetViewProjection) + 9];
            dmGameSystemDDF::SetViewProjection* set_view_projection = (dmGameSystemDDF::SetViewProjection*) (buf + sizeof(dmGameObject::InstanceMessageData));

            uint32_t socket_id = dmHashString32("render");
            uint32_t message_id = dmHashString32(dmGameSystemDDF::SetViewProjection::m_DDFDescriptor->m_ScriptName);

            const char* id = "game"; // TODO: How should this be handle, ddf-property in the camera?
            DM_SNPRINTF(buf + sizeof(dmGameObject::InstanceMessageData) + sizeof(dmGameSystemDDF::SetViewProjection), 9, "%X", dmHashString32(id));
            set_view_projection->m_Id = (const char*) sizeof(dmGameSystemDDF::SetViewProjection);
            set_view_projection->m_View = view;
            set_view_projection->m_Projection = projection;

            dmGameObject::InstanceMessageData* msg = (dmGameObject::InstanceMessageData*) buf;
            msg->m_BufferSize = sizeof(dmGameSystemDDF::SetViewProjection) + 9;
            msg->m_DDFDescriptor = dmGameSystemDDF::SetViewProjection::m_DDFDescriptor;
            msg->m_MessageId = message_id;

            dmMessage::Post(socket_id, message_id, buf, sizeof(buf));

            // Set matrices immediately
            // TODO: Remove this once render scripts are implemented everywhere
            dmRender::SetProjectionMatrix(render_context, projection);
            dmRender::SetViewMatrix(render_context, view);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCameraOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        Camera* camera = (Camera*)*user_data;
        if (message_data->m_DDFDescriptor == dmCameraDDF::SetCamera::m_DDFDescriptor)
        {
            dmCameraDDF::SetCamera* ddf = (dmCameraDDF::SetCamera*)message_data->m_Buffer;
            camera->m_AspectRatio = ddf->m_AspectRatio;
            camera->m_FOV = ddf->m_FOV;
            camera->m_NearZ = ddf->m_NearZ;
            camera->m_FarZ = ddf->m_FarZ;
        }
        else if (message_data->m_MessageId == dmHashString32("acquire_camera_focus"))
        {
            camera->m_World->m_FocusStack.Push(camera);
        }
        else if (message_data->m_MessageId == dmHashString32("release_camera_focus"))
        {
            for (uint32_t i = 0; i < camera->m_World->m_FocusStack.Size(); ++i)
            {
                if (camera->m_World->m_FocusStack[i] == camera)
                {
                    camera->m_World->m_FocusStack[i] = 0x0;
                    break;
                }
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
