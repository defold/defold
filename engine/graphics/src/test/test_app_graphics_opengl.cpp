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

#include "test_app_graphics_texture_upload.h"
#include <platform/window.hpp>
#include "opengl/graphics_opengl_defines.h"

namespace dmGraphicsTest
{
    bool ReadOpenGLTextureUpload(dmGraphics::HContext context, dmGraphics::HTexture texture, const TextureUploadReadback& params, dmArray<uint8_t>& pixels)
    {
        using namespace dmGraphics;
        typedef void (*GetImage)(GLenum, GLint, GLenum, GLenum, void*);
        typedef void (*GetCompressedImage)(GLenum, GLint, void*);
        GetImage get_image = (GetImage)dmPlatform::GetProcAddress(context->m_Window, "glGetTexImage");
        GetCompressedImage get_compressed = (GetCompressedImage)dmPlatform::GetProcAddress(context->m_Window, "glGetCompressedTexImage");
        void* handle = 0;
        if (!get_image || GetTextureHandle(texture, &handle) != HANDLE_RESULT_OK)
            return false;
        TextureType type = GetTextureType(context, texture);
        bool cube = type == TEXTURE_TYPE_CUBE_MAP;
        GLenum target = cube ? GL_TEXTURE_CUBE_MAP : type == TEXTURE_TYPE_2D_ARRAY ? GL_TEXTURE_2D_ARRAY :
        IsTextureType3D(type)                                                      ? GL_TEXTURE_3D :
                                                                                     GL_TEXTURE_2D;
        GLenum format = params.m_Format == TEXTURE_FORMAT_RGB || params.m_Format == TEXTURE_FORMAT_RGB16F || params.m_Format == TEXTURE_FORMAT_RGB32F ? GL_RGB : GL_RGBA;
        GLenum component_type = params.m_Format == TEXTURE_FORMAT_RGB16F || params.m_Format == TEXTURE_FORMAT_RGBA16F ? GL_HALF_FLOAT :
        params.m_Format == TEXTURE_FORMAT_RGB32F || params.m_Format == TEXTURE_FORMAT_RGBA32F                         ? GL_FLOAT :
                                                                                                                        GL_UNSIGNED_BYTE;
        bool compressed = IsTextureFormatCompressed(params.m_Format);
        if (compressed && !get_compressed)
            return false;
        glBindTexture(target, *(GLuint*)handle);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        uint32_t face_size = params.m_RowBytes * params.m_Rows;
        for (uint32_t face = 0; face < (cube ? 6U : 1U); ++face)
        {
            GLenum face_target = cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : target;
            if (compressed)
                get_compressed(face_target, params.m_Mip, pixels.Begin() + face * face_size);
            else
                get_image(face_target, params.m_Mip, format, component_type, pixels.Begin() + face * face_size);
        }
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glBindTexture(target, 0);
        return glGetError() == GL_NO_ERROR;
    }
} // namespace dmGraphicsTest
