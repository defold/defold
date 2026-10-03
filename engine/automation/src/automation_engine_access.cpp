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

#include "automation_engine_access.h"

#include <dmsdk/gui/gui.h>
#include <dmsdk/platform/window.h>
#include <dmsdk/render/render.h>

#include <float.h>
#include <math.h>
#include <string.h>

#include <graphics.h>
#include <window.hpp>
#include <render/render.h>
#include <gui/gui.h>
#include <gameobject/gameobject_private.h>
#include <gamesys/components/comp_gui.h>
#include <gamesys/components/comp_sprite.h>
#include <gamesys/components/comp_label.h>

namespace dmAutomation
{
    static dmResource::HFactory g_ResourceFactory;
    static dmHashTable64<uint32_t> g_ResourceVersions;
    static uint32_t g_ResourceVersion;

    static void ResourceReloaded(const dmResource::ResourceReloadedParams* params)
    {
        dmhash_t resource = ResourceDescriptorGetNameHash(params->m_Resource);
        if (!g_ResourceVersions.Get(resource) && g_ResourceVersions.Full())
        {
            uint32_t capacity = g_ResourceVersions.Capacity() + 32;
            g_ResourceVersions.SetCapacity(capacity * 2, capacity);
        }
        g_ResourceVersions.Put(resource, ++g_ResourceVersion);
        g_AutomationBridge.m_SnapshotFrame = UINT64_MAX;
    }

    uint32_t EngineAccessGetResourceVersion(void* resource)
    {
        dmhash_t path;
        if (!resource || dmResource::GetPath(g_ResourceFactory, resource, &path) != dmResource::RESULT_OK)
            return 0;
        uint32_t* version = g_ResourceVersions.Get(path);
        return version ? *version : 0;
    }

    uint64_t EngineAccessGetComponentVersion(const dmGameObject::SceneNode* node)
    {
        // Scene traversal already resolved these handles. Keep inspection-only
        // reload bookkeeping here instead of adding storage to every instance.
        dmGameObject::Instance* instance = dmGameObject::GetInstanceFromHandle(node->m_Instance);
        dmGameObject::Prototype::Component* component = (dmGameObject::Prototype::Component*)node->m_ComponentPrototype;
        return ((uint64_t)EngineAccessGetResourceVersion(instance->m_Prototype) << 32) |
               EngineAccessGetResourceVersion(component->m_Resource);
    }

    namespace
    {

        static bool IsUsableScreen(const Snapshot* snapshot)
        {
            return snapshot && snapshot->m_WindowWidth > 0 && snapshot->m_WindowHeight > 0;
        }

        static bool IsUsablePoint(float x, float y)
        {
            return IsFiniteFloat(x) && IsFiniteFloat(y);
        }

        static void InitAccumulator(float* min_x, float* min_y, float* max_x, float* max_y)
        {
            *min_x = FLT_MAX;
            *min_y = FLT_MAX;
            *max_x = -FLT_MAX;
            *max_y = -FLT_MAX;
        }

        static bool AccumulatePoint(float x, float y, float* min_x, float* min_y, float* max_x, float* max_y)
        {
            if (!IsUsablePoint(x, y))
            {
                return false;
            }
            *min_x = MinFloat(*min_x, x);
            *min_y = MinFloat(*min_y, y);
            *max_x = MaxFloat(*max_x, x);
            *max_y = MaxFloat(*max_y, y);
            return true;
        }

        static bool SetBoundsFromAccumulator(float min_x, float min_y, float max_x, float max_y, const Snapshot* snapshot, Bounds* out_bounds)
        {
            if (min_x == FLT_MAX || min_y == FLT_MAX || max_x == -FLT_MAX || max_y == -FLT_MAX)
            {
                return false;
            }

            out_bounds->m_X = min_x;
            out_bounds->m_Y = min_y;
            out_bounds->m_W = MaxFloat(0.0f, max_x - min_x);
            out_bounds->m_H = MaxFloat(0.0f, max_y - min_y);
            out_bounds->m_CX = min_x + out_bounds->m_W * 0.5f;
            out_bounds->m_CY = min_y + out_bounds->m_H * 0.5f;
            out_bounds->m_NX = out_bounds->m_CX / (float)snapshot->m_WindowWidth;
            out_bounds->m_NY = out_bounds->m_CY / (float)snapshot->m_WindowHeight;
            out_bounds->m_Valid = true;
            return true;
        }

        static bool IsDesktopWindowResizeSupported()
        {
#if defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_LINUX)
            return true;
#else
            return false;
#endif
        }

        static bool IsCloseDimension(uint32_t actual, uint32_t expected, float tolerance)
        {
            return fabsf((float)actual - (float)expected) <= tolerance;
        }

        static bool CurrentWindowSizeMatches(uint32_t width, uint32_t height, float scale)
        {
            if (!g_AutomationBridge.m_GraphicsContext)
            {
                return false;
            }

            uint32_t actual_width = dmGraphics::GetWindowWidth(g_AutomationBridge.m_GraphicsContext);
            uint32_t actual_height = dmGraphics::GetWindowHeight(g_AutomationBridge.m_GraphicsContext);
            float tolerance = MaxFloat(1.0f, ceilf(scale));
            return IsCloseDimension(actual_width, width, tolerance) &&
                   IsCloseDimension(actual_height, height, tolerance);
        }

        static bool ScreenToDebugWorld(const dmVMath::Matrix4& inverse_view_projection, uint32_t window_width, uint32_t window_height, float x, float y, dmVMath::Point3* out)
        {
            if (window_width == 0 || window_height == 0)
            {
                return false;
            }

            // HID coordinates address framebuffer pixels from the top-left. Aim
            // at the center of the exact pixel delivered to HID, then unproject
            // through the matrix DrawDebug3d will actually use. This also makes
            // the overlay independent of a render camera left active by the
            // user's render script.
            float ndc_x = 2.0f * ((x + 0.5f) / (float)window_width) - 1.0f;
            float ndc_y = 1.0f - 2.0f * ((y + 0.5f) / (float)window_height);
            dmVMath::Vector4 world = inverse_view_projection * dmVMath::Vector4(ndc_x, ndc_y, 0.0f, 1.0f);
            float w = world.getW();
            if (!IsFiniteFloat(w) || fabsf(w) < 0.000001f)
            {
                return false;
            }

            float inverse_w = 1.0f / w;
            float world_x = world.getX() * inverse_w;
            float world_y = world.getY() * inverse_w;
            float world_z = world.getZ() * inverse_w;
            if (!IsFiniteFloat(world_x) || !IsFiniteFloat(world_y) || !IsFiniteFloat(world_z))
            {
                return false;
            }

            *out = dmVMath::Point3(world_x, world_y, world_z);
            return true;
        }

        static void DrawDebugLine(dmRender::HRenderContext render_context, const dmVMath::Matrix4& inverse_view_projection, uint32_t window_width, uint32_t window_height, float x1, float y1, float x2, float y2, const dmVMath::Vector4& color)
        {
            dmVMath::Point3 start;
            dmVMath::Point3 end;
            if (!ScreenToDebugWorld(inverse_view_projection, window_width, window_height, x1, y1, &start) ||
                !ScreenToDebugWorld(inverse_view_projection, window_width, window_height, x2, y2, &end))
            {
                return;
            }

            dmRender::Line3D(render_context, start, end, color, color);
        }

        static void DrawDebugCircle(dmRender::HRenderContext render_context, const dmVMath::Matrix4& inverse_view_projection, uint32_t window_width, uint32_t window_height, float x, float y, float radius, const dmVMath::Vector4& color)
        {
            static const uint32_t SEGMENTS = 32;
            static const float TAU = 6.28318530717958647692f;

            float previous_x = x + radius;
            float previous_y = y;
            for (uint32_t i = 1; i <= SEGMENTS; ++i)
            {
                float angle = ((float)i / (float)SEGMENTS) * TAU;
                float next_x = x + cosf(angle) * radius;
                float next_y = y + sinf(angle) * radius;
                DrawDebugLine(render_context, inverse_view_projection, window_width, window_height,
                              previous_x, previous_y, next_x, next_y, color);
                previous_x = next_x;
                previous_y = next_y;
            }
        }

        static void AdvanceInputVisualization(InputVisualization* visualization)
        {
            uint64_t now = dmTime::GetMonotonicTime();
            if (visualization->m_LastRenderTime != 0)
            {
                visualization->m_Age += (float)((now - visualization->m_LastRenderTime) / 1000000.0);
                if (visualization->m_Age >= visualization->m_Duration)
                {
                    memset(visualization, 0, sizeof(*visualization));
                    return;
                }
            }
            visualization->m_LastRenderTime = now;
        }

        static bool ComputeGuiBounds(const dmGameObject::SceneNode* scene_node, const Node* node, const Snapshot* snapshot, Bounds* out_bounds)
        {
            if (!scene_node || scene_node->m_Component == 0 || scene_node->m_Node == 0)
            {
                return false;
            }

            dmGui::HScene scene = dmGameSystem::CompGuiGetScene(scene_node->m_Component);
            dmGui::HNode gui_node = (dmGui::HNode)scene_node->m_Node;
            if (!scene || gui_node == dmGui::INVALID_HANDLE)
            {
                return false;
            }

            dmRender::InspectionProjection projection;
            dmGameSystem::CompGuiGetInspectionProjection(g_AutomationBridge.m_RenderContext, scene_node->m_Component, &projection);
            out_bounds->m_Ambiguous = projection.m_State == 2;
            if (projection.m_State != 1) return false;
            dmVMath::Matrix4 transform = dmGui::GetNodeWorldTransform(scene, gui_node);
            dmVMath::Vector4 size = dmGui::GetNodeProperty(scene, gui_node, dmGui::PROPERTY_SIZE);
            float width = fabsf(size.getX());
            float height = fabsf(size.getY());

            float left = -width * node->m_AnchorX;
            float right = left + width;
            float bottom = -height * (1.0f - node->m_AnchorY);
            float top = bottom + height;

            const dmVMath::Vector4 points[] = {
                dmVMath::Vector4(left, bottom, 0.0f, 1.0f),
                dmVMath::Vector4(right, bottom, 0.0f, 1.0f),
                dmVMath::Vector4(right, top, 0.0f, 1.0f),
                dmVMath::Vector4(left, top, 0.0f, 1.0f),
            };

            float min_x;
            float min_y;
            float max_x;
            float max_y;
            InitAccumulator(&min_x, &min_y, &max_x, &max_y);

            for (uint32_t i = 0; i < DM_ARRAY_SIZE(points); ++i)
            {
                dmVMath::Vector4 point = transform * points[i];
                dmVMath::Vector4 clip = projection.m_ViewProjection * point;
                if (clip.getW() <= 0.000001f) return false;
                float screen_x = projection.m_ViewportX + (clip.getX() / clip.getW() + 1.0f) * 0.5f * projection.m_ViewportWidth;
                float screen_y = snapshot->m_WindowHeight - (projection.m_ViewportY + (clip.getY() / clip.getW() + 1.0f) * 0.5f * projection.m_ViewportHeight);
                if (!AccumulatePoint(screen_x, screen_y, &min_x, &min_y, &max_x, &max_y))
                {
                    return false;
                }
            }

            return SetBoundsFromAccumulator(min_x, min_y, max_x, max_y, snapshot, out_bounds);
        }

        static HWindow GetAutomationBridgeWindow()
        {
            if (!g_AutomationBridge.m_GraphicsContext)
            {
                return 0;
            }
            return dmGraphics::GetWindow(g_AutomationBridge.m_GraphicsContext);
        }

        static void SetGraphicsStateEnabled(dmGraphics::HContext graphics_context, dmGraphics::State state, bool enabled)
        {
            if (enabled)
            {
                dmGraphics::EnableState(graphics_context, state);
            }
            else
            {
                dmGraphics::DisableState(graphics_context, state);
            }
        }

        static void RestoreInputVisualizationGraphicsState(dmGraphics::HContext graphics_context, const dmGraphics::PipelineState& previous_state, int32_t viewport_x, int32_t viewport_y, uint32_t viewport_width, uint32_t viewport_height)
        {
            SetGraphicsStateEnabled(graphics_context, dmGraphics::STATE_BLEND, previous_state.m_BlendEnabled);
            SetGraphicsStateEnabled(graphics_context, dmGraphics::STATE_DEPTH_TEST, previous_state.m_DepthTestEnabled);
            SetGraphicsStateEnabled(graphics_context, dmGraphics::STATE_STENCIL_TEST, previous_state.m_StencilEnabled);
            SetGraphicsStateEnabled(graphics_context, dmGraphics::STATE_SCISSOR_TEST, previous_state.m_ScissorTestEnabled);
            SetGraphicsStateEnabled(graphics_context, dmGraphics::STATE_CULL_FACE, previous_state.m_CullFaceEnabled);
            SetGraphicsStateEnabled(graphics_context, dmGraphics::STATE_POLYGON_OFFSET_FILL, previous_state.m_PolygonOffsetFillEnabled);

            dmGraphics::SetBlendFuncSeparate(
                graphics_context,
                (dmGraphics::BlendFactor)previous_state.m_BlendSrcFactor,
                (dmGraphics::BlendFactor)previous_state.m_BlendDstFactor,
                (dmGraphics::BlendFactor)previous_state.m_BlendSrcFactorAlpha,
                (dmGraphics::BlendFactor)previous_state.m_BlendDstFactorAlpha);
            dmGraphics::SetBlendEquationSeparate(
                graphics_context,
                (dmGraphics::BlendEquation)previous_state.m_BlendEquationColor,
                (dmGraphics::BlendEquation)previous_state.m_BlendEquationAlpha);
            dmGraphics::SetDepthMask(graphics_context, previous_state.m_WriteDepth);
            dmGraphics::SetColorMask(
                graphics_context,
                (previous_state.m_WriteColorMask & (1 << 3)) != 0,
                (previous_state.m_WriteColorMask & (1 << 2)) != 0,
                (previous_state.m_WriteColorMask & (1 << 1)) != 0,
                (previous_state.m_WriteColorMask & (1 << 0)) != 0);
            dmGraphics::SetViewport(graphics_context, viewport_x, viewport_y, viewport_width, viewport_height);
        }

        static void PrepareInputVisualizationRenderState(dmGraphics::HContext graphics_context, dmRender::HRenderContext render_context, uint32_t window_width, uint32_t window_height)
        {
            dmGraphics::SetViewport(graphics_context, 0, 0, window_width, window_height);
            dmGraphics::EnableState(graphics_context, dmGraphics::STATE_BLEND);
            dmGraphics::DisableState(graphics_context, dmGraphics::STATE_DEPTH_TEST);
            dmGraphics::DisableState(graphics_context, dmGraphics::STATE_STENCIL_TEST);
            dmGraphics::DisableState(graphics_context, dmGraphics::STATE_SCISSOR_TEST);
            dmGraphics::DisableState(graphics_context, dmGraphics::STATE_CULL_FACE);
            dmGraphics::DisableState(graphics_context, dmGraphics::STATE_POLYGON_OFFSET_FILL);
            dmGraphics::SetColorMask(graphics_context, true, true, true, true);
            dmGraphics::SetDepthMask(graphics_context, false);
            dmGraphics::SetBlendFuncSeparate(
                graphics_context,
                dmGraphics::BLEND_FACTOR_SRC_ALPHA,
                dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                dmGraphics::BLEND_FACTOR_ONE,
                dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            dmGraphics::SetBlendEquationSeparate(
                graphics_context,
                dmGraphics::BLEND_EQUATION_ADD,
                dmGraphics::BLEND_EQUATION_ADD);

            dmRender::SetViewMatrix(render_context, dmVMath::Matrix4::identity());
            dmRender::SetProjectionMatrix(render_context, dmVMath::Matrix4::orthographic(0.0f, (float)window_width, 0.0f, (float)window_height, 1.0f, -1.0f));
        }

        static bool ResolveInputVisualizationMatrix(dmRender::HRenderContext render_context, dmVMath::Matrix4* inverse_view_projection)
        {
            // DrawRenderList reapplies m_CurrentRenderCamera after explicit
            // view/projection setup. An empty debug pass resolves that state
            // without drawing, so the real pass can use matching screen points.
            dmRender::RenderListBegin(render_context);
            dmRender::RenderListEnd(render_context);
            dmRender::Result result = dmRender::DrawDebug3d(render_context, 0);
            dmRender::ClearRenderObjects(render_context);
            if (result != dmRender::RESULT_OK)
            {
                return false;
            }

            *inverse_view_projection = dmVMath::Inverse(dmRender::GetViewProjectionMatrix(render_context));
            return true;
        }
    }

    void EngineAccessInitialize(dmExtension::Params* params)
    {
        if (!g_ResourceFactory && params)
        {
            g_ResourceFactory = (dmResource::HFactory)ExtensionParamsGetContextByName(params, RESOURCE_FACTORY_CONTEXT_NAME);
            dmResource::RegisterResourceReloadedCallback(g_ResourceFactory, ResourceReloaded, 0);
        }
        g_AutomationBridge.m_RenderContext = params ? (dmRender::HRenderContext)ExtensionParamsGetContextByName((ExtensionParams*)params, RENDER_CONTEXT_NAME) : 0;
        if (g_AutomationBridge.m_RenderContext) dmRender::EnableInspection(g_AutomationBridge.m_RenderContext, true);
    }

    void EngineAccessFinalize()
    {
        if (g_ResourceFactory)
            dmResource::UnregisterResourceReloadedCallback(g_ResourceFactory, ResourceReloaded, 0);
        g_ResourceFactory = 0;
        g_ResourceVersions.Clear();
        g_ResourceVersion = 0;
    }

    bool EngineAccessCanSetWindowSize()
    {
        return dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_NULL && IsDesktopWindowResizeSupported() && GetAutomationBridgeWindow() != 0;
    }

    bool EngineAccessSetWindowSize(uint32_t width, uint32_t height)
    {
        if (!EngineAccessCanSetWindowSize() || width == 0 || height == 0)
        {
            return false;
        }

        HWindow window = GetAutomationBridgeWindow();
        if (!window)
        {
            return false;
        }

        // Defold reports screen/window metadata in framebuffer pixels, while
        // GLFW's platform setter takes window coordinates on high-DPI displays.
        float scale = dmPlatform::GetDisplayScaleFactor(window);
        if (!IsFiniteFloat(scale) || scale < 1.0f)
        {
            scale = 1.0f;
        }

        uint32_t window_width = (uint32_t)MaxFloat(1.0f, floorf((float)width / scale + 0.5f));
        uint32_t window_height = (uint32_t)MaxFloat(1.0f, floorf((float)height / scale + 0.5f));
        dmPlatform::SetWindowSize(window, window_width, window_height);
        return CurrentWindowSizeMatches(width, height, scale);
    }

    bool EngineAccessComputeBounds(const dmGameObject::SceneNode* scene_node, const Node* node, const Snapshot* snapshot, Bounds* out_bounds)
    {
        if (!node || !out_bounds || !IsUsableScreen(snapshot) || dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_NULL)
        {
            return false;
        }

        memset(out_bounds, 0, sizeof(*out_bounds));

        if (StringsEqual(node->m_Kind, "gui_node") && ComputeGuiBounds(scene_node, node, snapshot, out_bounds))
        {
            return true;
        }

        if ((StringsEqual(node->m_Type, "spritec") || StringsEqual(node->m_Type, "labelc")) && scene_node->m_ComponentWorld)
        {
            dmVMath::Vector4 points[4];
            dmRender::InspectionProjection projection;
            bool valid = StringsEqual(node->m_Type, "spritec") ?
                dmGameSystem::CompSpriteGetInspectionBounds(g_AutomationBridge.m_RenderContext, scene_node->m_ComponentWorld, scene_node->m_Component, points, &projection) :
                dmGameSystem::CompLabelGetInspectionBounds(g_AutomationBridge.m_RenderContext, scene_node->m_ComponentWorld, scene_node->m_Component, points, &projection);
            if (!valid) return false;
            out_bounds->m_Ambiguous = projection.m_State == 2;
            if (projection.m_State != 1) return false;
            float min_x, min_y, max_x, max_y;
            InitAccumulator(&min_x, &min_y, &max_x, &max_y);
            for (uint32_t i = 0; i < 4; ++i)
            {
                dmVMath::Vector4 clip = projection.m_ViewProjection * points[i];
                if (clip.getW() <= 0.000001f) return false;
                float x = projection.m_ViewportX + (clip.getX() / clip.getW() + 1) * 0.5f * projection.m_ViewportWidth;
                float y = snapshot->m_WindowHeight - (projection.m_ViewportY + (clip.getY() / clip.getW() + 1) * 0.5f * projection.m_ViewportHeight);
                if (!AccumulatePoint(x, y, &min_x, &min_y, &max_x, &max_y)) return false;
            }
            return SetBoundsFromAccumulator(min_x, min_y, max_x, max_y, snapshot, out_bounds);
        }
        return false;
    }

    void EngineAccessDrawInputVisualization(InputVisualization* visualization)
    {
        if (!visualization || !visualization->m_Active || !g_AutomationBridge.m_RenderContext || !g_AutomationBridge.m_GraphicsContext)
        {
            return;
        }

        uint32_t window_width = dmGraphics::GetWindowWidth(g_AutomationBridge.m_GraphicsContext);
        uint32_t window_height = dmGraphics::GetWindowHeight(g_AutomationBridge.m_GraphicsContext);
        if (window_width == 0 || window_height == 0)
        {
            return;
        }

        int32_t previous_viewport_x = 0;
        int32_t previous_viewport_y = 0;
        uint32_t previous_viewport_width = 0;
        uint32_t previous_viewport_height = 0;
        dmGraphics::GetViewport(g_AutomationBridge.m_GraphicsContext, &previous_viewport_x, &previous_viewport_y, &previous_viewport_width, &previous_viewport_height);
        dmGraphics::PipelineState previous_state = dmGraphics::GetPipelineState(g_AutomationBridge.m_GraphicsContext);

        float duration = visualization->m_Duration > 0.0f ? visualization->m_Duration : 1.0f;
        float t = ClampFloat(visualization->m_Age / duration, 0.0f, 1.0f);
        float alpha = 1.0f - t;
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)g_AutomationBridge.m_RenderContext;
        dmVMath::Matrix4 previous_view = dmRender::GetViewMatrix(render_context);
        dmVMath::Matrix4 previous_view_projection = dmRender::GetViewProjectionMatrix(render_context);
        dmVMath::Matrix4 previous_projection = previous_view_projection * dmVMath::Inverse(previous_view);

        // The visualization is a final screen overlay. Always bind the
        // backbuffer in case the render script left an offscreen target active.
        // Value-initialize the binding argument for both the legacy uint32_t
        // mask and Defold 1.14.0's RenderTargetBindingParams.
        dmGraphics::SetRenderTarget(g_AutomationBridge.m_GraphicsContext, 0, {});
        PrepareInputVisualizationRenderState(g_AutomationBridge.m_GraphicsContext, render_context, window_width, window_height);

        dmVMath::Matrix4 inverse_view_projection;
        if (!ResolveInputVisualizationMatrix(render_context, &inverse_view_projection))
        {
            dmRender::SetViewMatrix(render_context, previous_view);
            dmRender::SetProjectionMatrix(render_context, previous_projection);
            RestoreInputVisualizationGraphicsState(g_AutomationBridge.m_GraphicsContext, previous_state, previous_viewport_x, previous_viewport_y, previous_viewport_width, previous_viewport_height);
            AdvanceInputVisualization(visualization);
            return;
        }

        dmRender::RenderListBegin(render_context);
        if (visualization->m_Drag)
        {
            dmVMath::Vector4 color(1.0f, 0.72f, 0.12f, alpha);
            for (uint32_t i = 1; i < visualization->m_PointCount; ++i)
            {
                DrawDebugLine(render_context, inverse_view_projection, window_width, window_height,
                              visualization->m_X[i - 1], visualization->m_Y[i - 1],
                              visualization->m_X[i], visualization->m_Y[i], color);
            }
            if (visualization->m_PointCount > 0)
            {
                DrawDebugCircle(render_context, inverse_view_projection, window_width, window_height,
                                visualization->m_X[0], visualization->m_Y[0], 6.0f, color);
                uint32_t last = visualization->m_PointCount - 1;
                DrawDebugCircle(render_context, inverse_view_projection, window_width, window_height,
                                visualization->m_X[last], visualization->m_Y[last], 6.0f, color);
            }
            dmRender::RenderListEnd(render_context);
            dmRender::DrawDebug3d(render_context, 0);
            dmRender::ClearRenderObjects(render_context);
            dmRender::SetViewMatrix(render_context, previous_view);
            dmRender::SetProjectionMatrix(render_context, previous_projection);
            RestoreInputVisualizationGraphicsState(g_AutomationBridge.m_GraphicsContext, previous_state, previous_viewport_x, previous_viewport_y, previous_viewport_width, previous_viewport_height);
            AdvanceInputVisualization(visualization);
            return;
        }

        float radius = 6.0f + 34.0f * t;
        dmVMath::Vector4 color(0.15f, 0.85f, 1.0f, alpha);
        if (visualization->m_PointCount > 0)
        {
            DrawDebugCircle(render_context, inverse_view_projection, window_width, window_height,
                            visualization->m_X[0], visualization->m_Y[0], radius, color);
        }
        dmRender::RenderListEnd(render_context);
        dmRender::DrawDebug3d(render_context, 0);
        dmRender::ClearRenderObjects(render_context);
        dmRender::SetViewMatrix(render_context, previous_view);
        dmRender::SetProjectionMatrix(render_context, previous_projection);
        RestoreInputVisualizationGraphicsState(g_AutomationBridge.m_GraphicsContext, previous_state, previous_viewport_x, previous_viewport_y, previous_viewport_width, previous_viewport_height);

        AdvanceInputVisualization(visualization);
    }

}
