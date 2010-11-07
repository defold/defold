#include "material.h"

#include <algorithm>
#include <string.h>

#include <dlib/log.h>

namespace dmGraphics
{
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

    struct Material
    {

        Material(): m_TagMask(0), m_FragmentConstantMask(0), m_VertexConstantMask(0)
        {
        }

        uint32_t                        m_TagMask;
        dmGraphics::HFragmentProgram    m_FragmentProgram;
        dmGraphics::HVertexProgram      m_VertexProgram;
        Vectormath::Aos::Vector4        m_FragmentConstant[MAX_MATERIAL_CONSTANTS];
        Vectormath::Aos::Vector4        m_VertexConstant[MAX_MATERIAL_CONSTANTS];

        uint8_t                         m_FragmentConstantMask;
        uint8_t                         m_VertexConstantMask;

    };


    HMaterial NewMaterial()
    {
        Material* m = new Material;
        return (HMaterial)m;
    }

    void DeleteMaterial(HMaterial material)
    {
        delete material;
    }

    void SetMaterialVertexProgram(HMaterial material, HVertexProgram vertexprogram)
    {
        material->m_VertexProgram = vertexprogram;
    }
    void SetMaterialFragmentProgram(HMaterial material, HFragmentProgram fragmentprogram)
    {
        material->m_FragmentProgram = fragmentprogram;
    }

    HVertexProgram GetMaterialVertexProgram(HMaterial material)
    {
        return material->m_VertexProgram;
    }

    HFragmentProgram GetMaterialFragmentProgram(HMaterial material)
    {
        return material->m_FragmentProgram;
    }

    void SetMaterialVertexProgramConstant(HMaterial material, uint32_t reg, Vector4 constant)
    {
        material->m_VertexConstant[reg] = constant;
    }

    void SetMaterialFragmentProgramConstant(HMaterial material, uint32_t reg, Vector4 constant)
    {
        material->m_FragmentConstant[reg] = constant;
    }

    Vector4 GetMaterialFragmentProgramConstant(HMaterial material, uint32_t reg)
    {
        return material->m_FragmentConstant[reg];
    }

    Vector4 GetMaterialVertexProgramConstant(HMaterial material, uint32_t reg)
    {
        return material->m_VertexConstant[reg];
    }

    void SetMaterialVertexConstantMask(HMaterial material, uint32_t mask)   { material->m_VertexConstantMask = mask; }
    void SetMaterialFragmentConstantMask(HMaterial material, uint32_t mask) { material->m_FragmentConstantMask = mask; }
    uint32_t GetMaterialVertexConstantMask(HMaterial material)              { return material->m_VertexConstantMask; }
    uint32_t GetMaterialFragmentConstantMask(HMaterial material)            { return material->m_FragmentConstantMask; }

    uint32_t GetMaterialTagMask(HMaterial material)
    {
        return material->m_TagMask;
    }

    void AddMaterialTag(HMaterial material, uint32_t tag)
    {
        Tag t;
        t.m_Tag = tag;
        Tag* begin = g_Tags;
        Tag* end = g_Tags + g_TagCount;
        Tag* result = std::lower_bound(begin, end, t, TagCompare);
        if (result != end && result->m_Tag == tag)
        {
            material->m_TagMask |= 1 << result->m_BitIndex;
        }
        else if (g_TagCount < MAX_TAG_COUNT)
        {
            g_Tags[g_TagCount].m_Tag = tag;
            g_Tags[g_TagCount].m_BitIndex = g_TagCount;
            material->m_TagMask |= 1 << g_TagCount;
            ++g_TagCount;
            std::sort(g_Tags, g_Tags + g_TagCount, TagCompare);
        }
        else
        {
            dmLogWarning("The material tag could not be registered since the maximum number of material tags (%d) has been reached.", MAX_TAG_COUNT);
        }
    }

    uint32_t ConvertMaterialTagsToMask(uint32_t* tags, uint32_t tag_count)
    {
        uint32_t mask = 0;
        for (uint32_t i = 0; i < tag_count; ++i)
        {
            Tag t;
            t.m_Tag = tags[i];
            Tag* begin = g_Tags;
            Tag* end = g_Tags + g_TagCount;
            Tag* result = std::lower_bound(begin, end, t, TagCompare);
            if (result != end && result->m_Tag == t.m_Tag)
            {
                mask |= 1 << result->m_BitIndex;
            }
            else
            {
                return 0;
            }
        }
        return mask;
    }
}
