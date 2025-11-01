// Copyright 2020-2025 The Defold Foundation
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
    // https://chromium.googlesource.com/external/github.com/ARM-software/astc-encoder/+/HEAD/Docs/FileFormat.md
    struct AstcHeader
    {
        uint8_t m_Magic[4];
        uint8_t m_BlockSizes[3];    // x, y, z block sizes (in texels)
        uint8_t m_DimensionX[3];    // X dimension (in texels). Encoded as: dim[0] + (dim[1] << 8) + (dim[2] << 16);
        uint8_t m_DimensionY[3];
        uint8_t m_DimensionZ[3];
    };

    struct Image
    {
        Image() : m_Width(0), m_Height(0), m_Type(TYPE_RGB), m_Buffer(0) {}
        uint32_t m_Width;
        uint32_t m_Height;
        Type     m_Type;
        void*    m_Buffer;
    };

    /**
     * Load image from buffer.
     *
     * <pre>
     *   Supported formats:
     *   png gray, gray + alpha, rgb and rgba
     *   jpg
     * </pre>
     * 16-bit (or higher) channels are not supported.
     *
     * @param buffer image buffer
     * @param buffer_size image buffer size
     * @param premult premultiply alpha or not
     * @param flip_vertically flip the image vertically
     * @param image output
     * @return RESULT_OK on success
     */
    Result Load(const void* buffer, uint32_t buffer_size, bool premult, bool flip_vertically, HImage image);

    /**
     * Free loaded image
     * @param image image to free
     */
    void Free(HImage image);

    /**
     * Get bytes per pixel
     * @param type
     * @return bytes per pixel. zero if the type is unknown
     */
    inline uint32_t BytesPerPixel(Type type)
    {
        switch (type)
        {
        case dmImage::TYPE_RGB:
            return 3;
        case dmImage::TYPE_RGBA:
            return 4;
        case dmImage::TYPE_LUMINANCE:
            return 1;
        case dmImage::TYPE_LUMINANCE_ALPHA:
            return 2;
        }
        return 0;
    }
}

#endif // #ifndef DM_IMAGE_H
