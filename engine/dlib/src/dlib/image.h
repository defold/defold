// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_IMAGE_H
#define DM_IMAGE_H

#include <stdint.h>

#include <dmsdk/dlib/image.h>

namespace dmImage
{
    struct Image
    {
        Image() : m_Width(0), m_Height(0), m_Type(TYPE_RGB), m_Buffer(0) {}
        uint32_t m_Width;
        uint32_t m_Height;
        Type     m_Type;
        void*    m_Buffer;
    };

    /**
     * Get bytes per pixel
     * @param type
     * @return bytes per pixel. zero if the type is unknown
     */
    uint32_t BytesPerPixel(Type type);
}

#endif // #ifndef DM_IMAGE_H
