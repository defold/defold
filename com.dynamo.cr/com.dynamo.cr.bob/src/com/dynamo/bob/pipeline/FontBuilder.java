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

package com.dynamo.bob.pipeline;

import java.io.IOException;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;

import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontRenderMode;
import com.dynamo.render.proto.Font.FontTextureFormat;

@ProtoParams(srcClass = FontDesc.class, messageClass = FontMap.class)
@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc")
public class FontBuilder extends ProtoBuilder<FontDesc.Builder> {

    private static final String DEFAULT_VECTOR_SDF_MATERIAL =
        "/builtins/fonts/font-df.material";
    private static final String DEFAULT_LABEL_VECTOR_SDF_MATERIAL =
        "/builtins/fonts/label-df.material";

    private static final String[] RUNTIME_FONT_RENDERER_MATERIALS = {
        "/builtins/fonts/font-df.material",
        "/builtins/fonts/font-vector-slug.material",
        "/builtins/fonts/font-vector-sweep.material",
    };

    private static boolean isTrueTypeFont(FontDesc fontDesc) {
        String path = fontDesc.getFont().toLowerCase();
        return path.endsWith(".ttf");
    }

    private static String getDefaultVectorSdfMaterial(FontDesc fontDesc) {
        String material = fontDesc.getMaterial();
        if (material.equals("/builtins/fonts/label-vector-slug.material") ||
            material.equals("/builtins/fonts/label-vector-slug-compat.material") ||
            material.equals("/builtins/fonts/label-vector-sweep.material") ||
            material.equals("/builtins/fonts/label-vector-sweep-compat.material")) {
            return DEFAULT_LABEL_VECTOR_SDF_MATERIAL;
        }
        return DEFAULT_VECTOR_SDF_MATERIAL;
    }

    private boolean useRuntimeGeneration(FontDesc fontDesc) {
        // TrueType fonts are represented by their vector outlines at runtime.
        // BMFont sources continue to use their supplied bitmap glyph pages.
        return isTrueTypeFont(fontDesc);
    }

    static FontDesc getEffectiveFontDesc(FontDesc fontDesc) {
        FontDesc.Builder builder = fontDesc.toBuilder();
        if (isTrueTypeFont(fontDesc)) {
            // TYPE_DISTANCE_FIELD remains the internal dynamic-font image format
            // until the legacy format enum is removed. Vector face rendering is
            // selected by the material's curve_texture sampler.
            builder.setOutputFormat(FontTextureFormat.TYPE_DISTANCE_FIELD);
            builder.setRenderMode(FontRenderMode.MODE_MULTI_LAYER);
            builder.clearCharacters();
            builder.setAllChars(false);
            // Vector outlines and visible shadows share the runtime SDF effect
            // texture and material, including hard shadows with zero blur.
            if (fontDesc.getShadowBlur() >= 1 ||
                fontDesc.getShadowAlpha() > 0.0f ||
                fontDesc.getOutlineWidth() > 0.0f) {
                if (fontDesc.getShadowMaterial().isEmpty()) {
                    builder.setShadowMaterial(getDefaultVectorSdfMaterial(fontDesc));
                }
            } else {
                builder.clearShadowMaterial();
            }
        } else {
            builder.setOutputFormat(FontTextureFormat.TYPE_BITMAP);
            builder.setRenderMode(FontRenderMode.MODE_SINGLE_LAYER);
            builder.clearShadowMaterial();
        }
        return builder.build();
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        FontDesc.Builder builder = getSrcBuilder(input);
        FontDesc fontDesc = getEffectiveFontDesc(builder.build());

        IResource fontResource = input.getResource(fontDesc.getFont());
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addOutput(input.changeExt(params.outExt()));

        // input(0)
        taskBuilder.addInput(input);

        // input(1)
        createSubTask(fontDesc.getMaterial(),"material", taskBuilder);

        Task subTask = null;
        if (useRuntimeGeneration(fontDesc))
        {
            // input(2)
            subTask = createSubTask(fontResource, CopyBuilders.TTFBuilder.class, taskBuilder);
            for (String material : RUNTIME_FONT_RENDERER_MATERIALS)
            {
                if (!material.equals(fontDesc.getMaterial()) &&
                    !material.equals(fontDesc.getShadowMaterial()))
                    createSubTask(material, "material", taskBuilder);
            }
        }
        else
        {
            // input(2)
            taskBuilder.addInput(fontResource);
            // input(3)
            subTask = createSubTask(input, GlyphBankBuilder.class, taskBuilder);
        }

        if (!fontDesc.getShadowMaterial().isEmpty()) {
            createSubTask(fontDesc.getShadowMaterial(), "shadow material", taskBuilder);
        }

        Task task = taskBuilder.build();
        subTask.setProductOf(task);
        return task;
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        FontDesc.Builder builder = getSrcBuilder(task.firstInput());
        FontDesc fontDesc = getEffectiveFontDesc(builder.build());
        FontMap.Builder fontMapBuilder = FontMap.newBuilder();

        BuilderUtil.checkResource(this.project, task.input(1), "material", fontDesc.getMaterial());
        if (useRuntimeGeneration(fontDesc))
        {
            BuilderUtil.checkResource(this.project, task.firstInput(), "font", fontDesc.getFont());
            // leave glyphbank field empty, as we use that to check at runtime (to toggle runtime generation or not)
            fontMapBuilder.setFont(fontDesc.getFont()); // Keep the suffix as-is (i.e. ".ttf")
        }
        else
        {
            String glyphBankPath   = BuilderUtil.getRelativePath(this.project, task.input(3));
            fontMapBuilder.setGlyphBank(glyphBankPath);
        }

        fontMapBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(fontDesc.getMaterial(), ".material", ".materialc"));
        if (!fontDesc.getShadowMaterial().isEmpty()) {
            fontMapBuilder.setShadowMaterial(ResourceUtil.minifyPathAndReplaceExt(fontDesc.getShadowMaterial(), ".material", ".materialc"));
        }
        if (useRuntimeGeneration(fontDesc))
        {
            for (String material : RUNTIME_FONT_RENDERER_MATERIALS)
            {
                fontMapBuilder.addRendererMaterials(
                    ResourceUtil.minifyPathAndReplaceExt(material, ".material", ".materialc"));
            }
        }

        if (fontDesc.getAllChars())
        {
            fontMapBuilder.setAllChars(true); // 0x000000 - 0x10FFFF
        }
        else
        {
            fontMapBuilder.setCharacters(fontDesc.getCharacters());
        }

        fontMapBuilder.setSize(fontDesc.getSize());
        fontMapBuilder.setAntialias(fontDesc.getAntialias());
        fontMapBuilder.setShadowX(fontDesc.getShadowX());
        fontMapBuilder.setShadowY(fontDesc.getShadowY());
        fontMapBuilder.setShadowBlur(fontDesc.getShadowBlur());
        fontMapBuilder.setShadowAlpha(fontDesc.getShadowAlpha());
        fontMapBuilder.setAlpha(fontDesc.getAlpha());
        fontMapBuilder.setOutlineAlpha(fontDesc.getOutlineAlpha());
        fontMapBuilder.setOutlineWidth(fontDesc.getOutlineWidth());
        fontMapBuilder.setLayerMask(Fontc.GetFontMapLayerMask(fontDesc));
        fontMapBuilder.setCacheWidth(fontDesc.getCacheWidth());
        fontMapBuilder.setCacheHeight(fontDesc.getCacheHeight());

        if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD)
        {
            fontMapBuilder.setSdfSpread(Fontc.GetFontMapSdfSpread(fontDesc));
            fontMapBuilder.setSdfOutline(Fontc.GetFontMapSdfOutline(fontDesc));
            fontMapBuilder.setSdfShadow(Fontc.GetFontMapSdfShadow(fontDesc));
        }

        fontMapBuilder.setOutputFormat(fontDesc.getOutputFormat());
        fontMapBuilder.setRenderMode(fontDesc.getRenderMode());

        task.output(0).setContent(fontMapBuilder.build().toByteArray());
    }
}
