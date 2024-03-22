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

#ifndef DMSDK_IMAGE
#define DMSDK_IMAGE

#include <stdint.h>

/*# SDK Image API documentation
 *
 * Image API functions.
 *
 * @document
 * @name Image
 * @namespace dmImage
 * @path engine/dlib/src/dmsdk/dlib/image.h
 */

namespace dmImage
{
    struct Image;
    typedef Image* HImage;

    /*# result enumeration
     *
     * @enum
     * @name Result
     * @member dmImage::RESULT_OK = 0
     * @member dmImage::RESULT_UNSUPPORTED_FORMAT = -1
     * @member dmImage::RESULT_IMAGE_ERROR = -2
     */
    enum Result
    {
        RESULT_OK                   = 0,
        RESULT_UNSUPPORTED_FORMAT   = -1,
        RESULT_IMAGE_ERROR          = -2,
    };

    /*# type enumeration
     *
     * @enum
     * @name Type
     * @member dmImage::TYPE_RGB = 0
     * @member dmImage::TYPE_RGBA = 1
     * @member dmImage::TYPE_LUMINANCE = 2
     * @member dmImage::TYPE_LUMINANCE_ALPHA = 3
     */
    enum Type
    {
        TYPE_RGB             = 0,
        TYPE_RGBA            = 1,
        TYPE_LUMINANCE       = 2,
        TYPE_LUMINANCE_ALPHA = 3,
    };

    /**
     * Create a new image
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
     * @return image [type:HImage] on success, 0 if image loading failed
     */
    HImage NewImage(const void* buffer, uint32_t buffer_size, bool premult);

    /**
     * Delete image
     * @param image [type: dmImage::HImage] the image handle
     */
    void DeleteImage(HImage image);

    /**
     * Get image type
     * @param image [type: dmImage::HImage] the image handle
     * @return type [type: Type] the image type
     */
    Type GetType(HImage image);

    /**
     * Get image width
     * @param image [type: dmImage::HImage] the image handle
     * @return width [type: uint32_t] the image width
     */
    uint32_t GetWidth(HImage image);

    /**
     * Get image height
     * @param image [type: dmImage::HImage] the image handle
     * @return height [type: uint32_t] the image height
     */
    uint32_t GetHeight(HImage image);

    /**
     * Get data pointer
     * @param image [type: dmImage::HImage] the image handle
     * @return data [type: uint32_t] the image height
     */
    const void* GetData(HImage image);
}

#endif // DMSDK_IMAGE
