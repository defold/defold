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

#ifndef DM_TEST_GUI_SHARED_H
#define DM_TEST_GUI_SHARED_H

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/image.h>

#include <stdint.h>

struct TestDynamicTexture
{
    dmhash_t      m_PathHash;
    void*         m_Buffer;
    dmImage::Type m_Type;
    uint32_t      m_Width;
    uint32_t      m_Height;
};


// This is just a wrapper around a list of TestDynamicTextures,
// that will grow as needed. Empty slots are reused.
// it could be rewritten to use a hash table, but it's just a
// small test class so it doesn't really matter.
class DynamicTextureContainer
{
public:
    ~DynamicTextureContainer()
    {
        for (int i = 0; i < m_Textures.Size(); ++i)
        {
            if (!m_Textures[i])
                continue;

            if (m_Textures[i]->m_Buffer)
            {
                free(m_Textures[i]->m_Buffer);
            }
            free((void*)m_Textures[i]);
        }
    };

    TestDynamicTexture* New(dmhash_t path_hash, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer) {
        uint32_t buffer_size = width * height * dmImage::BytesPerPixel(type);
        TestDynamicTexture* p = (TestDynamicTexture*) malloc(sizeof(TestDynamicTexture));
        p->m_Width = width;
        p->m_Height = height;
        p->m_Buffer = (uint8_t*) malloc(buffer_size);
        p->m_PathHash = path_hash;
        p->m_Type = type;
        memcpy(p->m_Buffer, buffer, buffer_size);

        // reuse empty slots
        for (int i = 0; i < m_Textures.Size(); ++i)
        {
            if (m_Textures[i] == 0)
            {
                m_Textures[i] = p;
                return p;
            }
        }

        // grow and store otherwise
        if (m_Textures.Full())
        {
            m_Textures.OffsetCapacity(1);
        }

        m_Textures.Push(p);

        return p;
    }

    void Delete(dmhash_t path_hash)
    {
        for (int i = 0; i < m_Textures.Size(); ++i)
        {
            if (m_Textures[i]->m_PathHash == path_hash)
            {
                free(m_Textures[i]->m_Buffer);
                free(m_Textures[i]);
                m_Textures[i] = 0;
            }
        }
    }

    void Set(dmhash_t path_hash, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer)
    {
        for (int i = 0; i < m_Textures.Size(); ++i)
        {
            if (m_Textures[i]->m_PathHash == path_hash)
            {
                uint32_t buffer_size = width * height * dmImage::BytesPerPixel(type);
                if (m_Textures[i]->m_Buffer)
                {
                    free(m_Textures[i]->m_Buffer);
                    m_Textures[i]->m_Buffer = (uint8_t*) malloc(buffer_size);
                }
                m_Textures[i]->m_Width = width;
                m_Textures[i]->m_Height = height;
                m_Textures[i]->m_Type = type;
                memcpy(m_Textures[i]->m_Buffer, buffer, buffer_size);
            }
        }
    }

    TestDynamicTexture* Get(dmhash_t path_hash)
    {
        for (int i = 0; i < m_Textures.Size(); ++i)
        {
            if (m_Textures[i] && m_Textures[i]->m_PathHash == path_hash)
            {
                return m_Textures[i];
            }
        }
        return 0;
    }

    dmArray<TestDynamicTexture*> m_Textures;
};

#endif // DM_TEST_GUI_SHARED_H
