// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0.

#include "render_private.h"

#include <dlib/log.h>

namespace dmRender
{
    static const dmhash_t CLUSTER_BUFFER_NAMES[CLUSTER_BUFFER_COUNT] =
    {
        dmHashString64("ClusterBoundsBuffer"),
        dmHashString64("ClusterMetadataBuffer"),
        dmHashString64("ClusterLightIndicesBuffer"),
        dmHashString64("ClusterCountersBuffer"),
        dmHashString64("ClusterOverflowBuffer")
    };

    struct ClusterBindingCallbackContext
    {
        ClusterBufferBinding* m_Bindings;
    };

    static void ClusterBindingCallback(uint16_t set, uint16_t binding,
                                       const dmGraphics::ShaderResourceTypeInfo* types,
                                       uint32_t num_types, uint32_t root_type_index,
                                       dmGraphics::UniformBufferLayout* layout,
                                       void* user_data)
    {
        if (root_type_index >= num_types)
            return;

        ClusterBindingCallbackContext* context = (ClusterBindingCallbackContext*) user_data;
        dmhash_t name_hash = types[root_type_index].m_NameHash;
        for (uint32_t i = 0; i < CLUSTER_BUFFER_COUNT; ++i)
        {
            if (name_hash == CLUSTER_BUFFER_NAMES[i])
            {
                context->m_Bindings[i].m_Set = set;
                context->m_Bindings[i].m_Binding = binding;
                context->m_Bindings[i].m_Present = 1;
                return;
            }
        }
    }

    void GetProgramClusterBufferBindings(dmGraphics::HProgram program, ClusterBufferBinding out_bindings[CLUSTER_BUFFER_COUNT])
    {
        memset(out_bindings, 0, sizeof(ClusterBufferBinding) * CLUSTER_BUFFER_COUNT);
        ClusterBindingCallbackContext context = { out_bindings };
        dmGraphics::IterateProgramResourceBindings(program, dmGraphics::BINDING_FAMILY_STORAGE_BUFFER,
                                                   ClusterBindingCallback, &context);
    }

    static void ApplyClusterBuffers(HRenderContext render_context, const ClusterBufferBinding bindings[CLUSTER_BUFFER_COUNT])
    {
        for (uint32_t i = 0; i < CLUSTER_BUFFER_COUNT; ++i)
        {
            if (bindings[i].m_Present && render_context->m_ClusterBuffers[i])
            {
                dmGraphics::EnableUniformBufferAsStorage(render_context->m_GraphicsContext,
                                                         render_context->m_ClusterBuffers[i],
                                                         bindings[i].m_Set,
                                                         bindings[i].m_Binding);
            }
        }
    }

    void ApplyMaterialClusterBuffers(HRenderContext render_context, HMaterial material)
    {
        ApplyClusterBuffers(render_context, material->m_ClusterBufferBindings);
    }

    void ApplyComputeProgramClusterBuffers(HRenderContext render_context, HComputeProgram compute_program)
    {
        ApplyClusterBuffers(render_context, compute_program->m_ClusterBufferBindings);
    }

    static bool RecreateClusterBuffer(HRenderContext context, ClusterBufferType type, uint32_t size)
    {
        if (context->m_ClusterBuffers[type] && context->m_ClusterBufferSizes[type] == size)
            return true;

        if (context->m_ClusterBuffers[type])
            dmGraphics::DeleteUniformBuffer(context->m_GraphicsContext, context->m_ClusterBuffers[type]);

        context->m_ClusterBuffers[type] = dmGraphics::NewUniformBuffer(context->m_GraphicsContext, 0, size);
        context->m_ClusterBufferSizes[type] = context->m_ClusterBuffers[type] ? size : 0;
        return context->m_ClusterBuffers[type] != 0;
    }

    bool SetClusteredLightingGrid(HRenderContext context, uint32_t x, uint32_t y, uint32_t z, uint32_t max_lights_per_cluster)
    {
        if (dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_VULKAN &&
            dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_NULL)
        {
            dmLogError("Clustered lighting storage buffers currently require Vulkan.");
            return false;
        }
        if (x == 0 || y == 0 || z == 0 || max_lights_per_cluster == 0)
            return false;

        uint64_t cluster_count_64 = (uint64_t) x * y * z;
        uint64_t index_count_64 = cluster_count_64 * max_lights_per_cluster;
        if (cluster_count_64 > UINT32_MAX / 32u || index_count_64 > UINT32_MAX / sizeof(uint32_t))
        {
            dmLogError("Clustered lighting grid is too large.");
            return false;
        }

        uint32_t cluster_count = (uint32_t) cluster_count_64;
        const uint32_t sizes[CLUSTER_BUFFER_COUNT] =
        {
            cluster_count * 32u,                       // two vec4 AABB corners
            cluster_count * 8u,                        // offset + count
            (uint32_t) index_count_64 * 4u,
            sizeof(uint32_t) * 4,                      // allocator + global diagnostics
            cluster_count * 4u                         // dropped lights per cluster
        };

        for (uint32_t i = 0; i < CLUSTER_BUFFER_COUNT; ++i)
        {
            if (!RecreateClusterBuffer(context, (ClusterBufferType) i, sizes[i]))
            {
                FinalizeClusteredLighting(context);
                return false;
            }
        }

        context->m_ClusterDimensions[0] = x;
        context->m_ClusterDimensions[1] = y;
        context->m_ClusterDimensions[2] = z;
        context->m_MaxLightsPerCluster = max_lights_per_cluster;
        ResetClusteredLightingBuffers(context);
        return true;
    }

    uint32_t GetClusteredLightingClusterCount(HRenderContext context)
    {
        return context->m_ClusterDimensions[0] * context->m_ClusterDimensions[1] * context->m_ClusterDimensions[2];
    }

    void ResetClusteredLightingBuffers(HRenderContext context)
    {
        const ClusterBufferType reset_buffers[] =
        {
            CLUSTER_BUFFER_METADATA,
            CLUSTER_BUFFER_COUNTERS,
            CLUSTER_BUFFER_OVERFLOW
        };
        dmArray<uint8_t> zeros;
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(reset_buffers); ++i)
        {
            ClusterBufferType type = reset_buffers[i];
            uint32_t size = context->m_ClusterBufferSizes[type];
            if (!size || !context->m_ClusterBuffers[type])
                continue;
            zeros.SetCapacity(size);
            zeros.SetSize(size);
            memset(zeros.Begin(), 0, size);
            dmGraphics::SetUniformBuffer(context->m_GraphicsContext, context->m_ClusterBuffers[type], 0, size, zeros.Begin());
        }
    }

    void FinalizeClusteredLighting(HRenderContext context)
    {
        for (uint32_t i = 0; i < CLUSTER_BUFFER_COUNT; ++i)
        {
            if (context->m_ClusterBuffers[i])
            {
                dmGraphics::DeleteUniformBuffer(context->m_GraphicsContext, context->m_ClusterBuffers[i]);
                context->m_ClusterBuffers[i] = 0;
            }
            context->m_ClusterBufferSizes[i] = 0;
        }
        memset(context->m_ClusterDimensions, 0, sizeof(context->m_ClusterDimensions));
        context->m_MaxLightsPerCluster = 0;
    }
}
