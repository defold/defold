#include <algorithm>
#include <string.h>

#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include "render.h"
#include "render_private.h"

namespace dmRender
{
    using namespace Vectormath::Aos;

    static const uint32_t MAX_TAG_COUNT = 32;
    struct Tag
    {
        uint32_t m_Tag;
        uint32_t m_BitIndex;
    };

    Tag g_Tags[MAX_TAG_COUNT];
    uint32_t g_TagCount = 0;

    bool TagCompare(const Tag& lhs, const Tag& rhs)
    {
        return lhs.m_Tag < rhs.m_Tag;
    }

    HMaterial NewMaterial(dmRender::HRenderContext render_context, dmGraphics::HVertexProgram vertex_program, dmGraphics::HFragmentProgram fragment_program)
    {
        Material* m = new Material;
        m->m_RenderContext = render_context;
        m->m_VertexProgram = vertex_program;
        m->m_FragmentProgram = fragment_program;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        m->m_Program = dmGraphics::NewProgram(graphics_context, vertex_program, fragment_program);

        uint32_t total_constants_count = dmGraphics::GetUniformCount(m->m_Program);
        char buffer[128];
        dmGraphics::Type type;

        uint32_t constants_count = 0;
        uint32_t samplers_count = 0;
        for (uint32_t i = 0; i < total_constants_count; ++i)
        {
            dmGraphics::GetUniformName(m->m_Program, i, buffer, sizeof(buffer), &type);

            if (type == dmGraphics::TYPE_FLOAT_VEC4 || type == dmGraphics::TYPE_FLOAT_MAT4)
            {
                constants_count++;
            }
            else if (type == dmGraphics::TYPE_SAMPLER_2D)
            {
                samplers_count++;
            }
            else
            {
                dmLogWarning("Type of uniform %s is not supported (%d)", buffer, type);
            }
        }

        if (constants_count > 0)
        {
            m->m_NameHashToLocation.SetCapacity(constants_count * 2, constants_count);
            m->m_Constants.SetCapacity(constants_count);
        }

        if (samplers_count > 0)
        {
            m->m_Samplers.SetCapacity(samplers_count);
        }

        for (uint32_t i = 0; i < total_constants_count; ++i)
        {
            dmGraphics::GetUniformName(m->m_Program, i, buffer, sizeof(buffer), &type);
            int32_t location = dmGraphics::GetUniformLocation(m->m_Program, buffer);
            assert(location != -1);
            dmhash_t name_hash = dmHashString64(buffer);

            if (type == dmGraphics::TYPE_FLOAT_VEC4 || type == dmGraphics::TYPE_FLOAT_MAT4)
            {
                m->m_NameHashToLocation.Put(name_hash, location);
                m->m_Constants.Push(Constant(name_hash, location));
            }
            else if (type == dmGraphics::TYPE_SAMPLER_2D)
            {
                m->m_Samplers.Push(Sampler(name_hash, location));
            }
        }

        return (HMaterial)m;
    }

    void DeleteMaterial(dmRender::HRenderContext render_context, HMaterial material)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        dmGraphics::DeleteProgram(graphics_context, material->m_Program);
        delete material;
    }

    void ApplyMaterialConstants(dmRender::HRenderContext render_context, HMaterial material, const RenderObject* ro)
    {
        const dmArray<Constant>& constants = material->m_Constants;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        uint32_t n = constants.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            const Constant& constant = constants[i];
            int32_t location = constant.m_Location;
            switch (constant.m_Type)
            {
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                {
                    dmGraphics::SetConstantV4(graphics_context, &constant.m_Value, location);
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ:
                {
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&render_context->m_ViewProj, location);
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD:
                {
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&ro->m_WorldTransform, location);
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TEXTURE:
                {
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&ro->m_TextureTransform, location);
                    break;
                }
            }
        }
    }

    void ApplyMaterialSamplers(dmRender::HRenderContext render_context, HMaterial material)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        dmArray<Sampler>& samplers = material->m_Samplers;
        uint32_t n = samplers.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            const Sampler& s = samplers[i];
            if (s.m_Unit != -1)
            {
                dmGraphics::SetSampler(graphics_context, s.m_Location, s.m_Unit);
            }
        }
    }

    dmGraphics::HProgram GetMaterialProgram(HMaterial material)
    {
        return material->m_Program;
    }

    dmGraphics::HVertexProgram GetMaterialVertexProgram(HMaterial material)
    {
        return material->m_VertexProgram;
    }

    dmGraphics::HFragmentProgram GetMaterialFragmentProgram(HMaterial material)
    {
        return material->m_FragmentProgram;
    }

    void SetMaterialProgramConstantType(HMaterial material, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type)
    {
        dmArray<Constant>& constants = material->m_Constants;
        uint32_t n = constants.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                c.m_Type = type;
                return;
            }
        }
    }

    bool GetMaterialProgramConstant(HMaterial material, dmhash_t name_hash, Constant& out_value)
    {
        dmArray<Constant>& constants = material->m_Constants;
        uint32_t n = constants.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                out_value = c;
                return true;
            }
        }
        return false;
    }

    void SetMaterialProgramConstant(HMaterial material, dmhash_t name_hash, Vector4 value)
    {
        dmArray<Constant>& constants = material->m_Constants;
        uint32_t n = constants.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                c.m_Value = value;
            }
        }
    }

    int32_t GetMaterialConstantLocation(HMaterial material, dmhash_t name_hash)
    {
        int32_t* location = material->m_NameHashToLocation.Get(name_hash);
        if (location)
            return *location;
        else
            return -1;
    }

    void SetMaterialSampler(HMaterial material, dmhash_t name_hash, int16_t unit)
    {
        dmArray<Sampler>& samplers = material->m_Samplers;
        uint32_t n = samplers.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Sampler& s = samplers[i];
            if (s.m_NameHash == name_hash)
            {
                s.m_Unit = unit;
            }
        }
    }

    HRenderContext GetMaterialRenderContext(HMaterial material)
    {
        return material->m_RenderContext;
    }

    uint64_t GetMaterialUserData1(HMaterial material)
    {
        return material->m_UserData1;
    }

    void SetMaterialUserData1(HMaterial material, uint64_t user_data)
    {
        material->m_UserData1 = user_data;
    }

    uint64_t GetMaterialUserData2(HMaterial material)
    {
        return material->m_UserData2;
    }

    void SetMaterialUserData2(HMaterial material, uint64_t user_data)
    {
        material->m_UserData2 = user_data;
    }

    uint32_t GetMaterialTagMask(HMaterial material)
    {
        return material->m_TagMask;
    }

    uint32_t ConvertTagToBitfield(uint32_t tag)
    {
        Tag t;
        t.m_Tag = tag;
        Tag* begin = g_Tags;
        Tag* end = g_Tags + g_TagCount;
        Tag* result = std::lower_bound(begin, end, t, TagCompare);
        if (result != end && result->m_Tag == tag)
        {
            return 1 << result->m_BitIndex;
        }
        else if (g_TagCount < MAX_TAG_COUNT)
        {
            g_Tags[g_TagCount].m_Tag = tag;
            g_Tags[g_TagCount].m_BitIndex = g_TagCount;
            uint32_t result = 1 << g_TagCount;
            ++g_TagCount;
            std::sort(g_Tags, g_Tags + g_TagCount, TagCompare);
            return result;
        }
        else
        {
            dmLogWarning("The material tag could not be registered since the maximum number of material tags (%d) has been reached.", MAX_TAG_COUNT);
            return 0;
        }
    }

    void AddMaterialTag(HMaterial material, uint32_t tag)
    {
        material->m_TagMask |= ConvertTagToBitfield(tag);
    }

    void ClearMaterialTags(HMaterial material)
    {
        material->m_TagMask = 0;
    }

    uint32_t ConvertMaterialTagsToMask(uint32_t* tags, uint32_t tag_count)
    {
        uint32_t mask = 0;
        for (uint32_t i = 0; i < tag_count; ++i)
        {
            mask |= ConvertTagToBitfield(tags[i]);
        }
        return mask;
    }
}
