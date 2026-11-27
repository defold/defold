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

#ifndef DM_TEST_TEXTURE_UPLOAD_VALIDATION_H
#define DM_TEST_TEXTURE_UPLOAD_VALIDATION_H

// Run on real adapters: null cannot reproduce explicit APIs skipping unavailable
// mips or asserting when a partial upload tries to create the texture storage.
struct TextureUploadValidationTest : ITest
{
    void Check(EngineCtx* engine, bool condition, const char* message)
    {
        if (!condition)
        {
            dmLogError("Texture upload validation: %s", message);
            engine->m_Failed = true;
        }
    }

    void Initialize(EngineCtx*) override
    {
    }

    void Execute(EngineCtx* engine) override
    {
        using namespace dmGraphics;
        HContext context = engine->m_GraphicsContext;
        AdapterFamily adapter = GetInstalledAdapterFamily();
        const bool separate_mips = adapter == ADAPTER_FAMILY_OPENGL || adapter == ADAPTER_FAMILY_OPENGLES;
        uint8_t pixels[256] = {};

        for (uint32_t mip_count = 1; mip_count <= 3; mip_count += 2)
        {
            TextureCreationParams creation;
            creation.m_Width = 4;
            creation.m_Height = 4;
            creation.m_MipMapCount = mip_count;
            HTexture texture = NewTexture(context, creation);

            TextureParams upload;
            upload.m_Width = 4;
            upload.m_Height = 4;
            upload.m_Data = pixels;
            upload.m_DataSize = 64;
            upload.m_SubUpdate = true;
            Check(engine, !IsTextureUploadValid(context, texture, upload), "partial upload accepted before storage exists");

            upload.m_SubUpdate = false;
            Check(engine, IsTextureUploadValid(context, texture, upload), "initial full upload rejected");
            SetTexture(context, texture, upload);

            upload.m_MipMap = 1;
            upload.m_Width = 2;
            upload.m_Height = 2;
            upload.m_DataSize = 16;
            upload.m_SubUpdate = true;
            Check(engine, IsTextureUploadValid(context, texture, upload) == (!separate_mips && mip_count > 1),
                  "partial mip upload did not follow the allocated storage");

            upload.m_SubUpdate = false;
            const bool can_upload_mip = separate_mips || mip_count > 1;
            Check(engine, IsTextureUploadValid(context, texture, upload) == can_upload_mip,
                  "full mip upload did not follow the adapter allocation rules");
            if (can_upload_mip)
            {
                SetTexture(context, texture, upload);
                upload.m_SubUpdate = true;
                Check(engine, IsTextureUploadValid(context, texture, upload), "allocated mip rejected");
                SetTexture(context, texture, upload);
            }

            upload.m_SubUpdate = false;
            upload.m_MipMap = 0;
            upload.m_Width = 8;
            upload.m_Height = 8;
            upload.m_DataSize = sizeof(pixels);
            Check(engine, IsTextureUploadValid(context, texture, upload), "full resize rejected");
            SetTexture(context, texture, upload);

            upload.m_SubUpdate = true;
            upload.m_X = 7;
            Check(engine, !IsTextureUploadValid(context, texture, upload), "out-of-bounds partial upload accepted");
            DeleteTexture(context, texture);
        }

        if (!engine->m_Failed)
        {
            dmLogInfo("Texture upload validation: PASS (initialization, allocated mips, partial updates and resize)");
        }

        engine->m_Running = 0;
    }

};

#endif
