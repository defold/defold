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

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.FontRenderer;
import com.dynamo.bob.font.BMFont;
import com.dynamo.bob.font.BMFont.BMFontFormatException;
import com.dynamo.bob.font.FontStyles;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;

import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontRenderMode;
import com.dynamo.render.proto.Font.FontTextureFormat;
import com.dynamo.render.proto.Font.VectorFontMode;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.google.protobuf.TextFormat;

@ProtoParams(srcClass = FontDesc.class, messageClass = FontMap.class)
@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc", paramsForSignature = {"font-runtime-generation"})
public class FontBuilder extends ProtoBuilder<FontDesc.Builder> {

    private static boolean isTrueTypeFont(FontDesc fontDesc) {
        String path = fontDesc.getFont().toLowerCase();
        return path.endsWith(".ttf") || path.endsWith(".otf");
    }

    private static boolean isBitmapFont(FontDesc fontDesc) {
        return fontDesc.getFont().toLowerCase().endsWith(".fnt");
    }

    private static boolean isVectorFont(FontDesc fontDesc) {
        return isTrueTypeFont(fontDesc) &&
               fontDesc.getVectorFontMode() == VectorFontMode.VECTOR_FONT_MODE_VECTOR;
    }

    private static boolean hasOutline(FontDesc fontDesc) {
        return fontDesc.getOutlineAlpha() > 0.0f &&
               (isBitmapFont(fontDesc) || fontDesc.getOutlineWidth() > 0.0f);
    }

    private static boolean hasShadow(FontDesc fontDesc) {
        return fontDesc.getShadowAlpha() > 0.0f &&
               (isBitmapFont(fontDesc) || fontDesc.getShadowBlur() > 0 ||
                fontDesc.getShadowX() != 0.0f || fontDesc.getShadowY() != 0.0f);
    }

    static FontDesc readFontDesc(Project project, String path) throws IOException {
        FontDesc.Builder desc = FontDesc.newBuilder();
        TextFormat.merge(new String(project.getResource(path).getContent(), StandardCharsets.UTF_8), desc);
        return desc.build();
    }

    static void validateTextEffects(IResource input, FontDesc fontDesc, String text, boolean richText,
                                   boolean outlineChanged, boolean shadowChanged) throws CompileExceptionError {
        if (!isVectorFont(fontDesc))
            return;

        if (richText && text.indexOf('<') >= 0) {
            FontRenderer.MarkupParseResult parsed = FontRenderer.parseMarkup(text);
            if (parsed.document != null) {
                for (FontRenderer.MarkupNode node : parsed.document.nodes) {
                    outlineChanged |= node.tag.equals("outline");
                    shadowChanged |= node.tag.equals("shadow");
                }
            }
        }
        if (outlineChanged && !hasOutline(fontDesc)) {
            throw new CompileExceptionError(input, 0,
                "The font has no outline enabled. Enable Outline Alpha and Outline Width and set Size in the .font resource.");
        }
        if (shadowChanged && !hasShadow(fontDesc)) {
            throw new CompileExceptionError(input, 0,
                "The font has no shadow enabled. Enable Shadow Alpha and a shadow offset or blur and set Size in the .font resource.");
        }
    }

    private boolean legacyRuntimeGeneration() {
        return this.project.option("font-runtime-generation", "false").equals("true");
    }

    private static boolean useRuntimeGeneration(FontDesc fontDesc) {
        return isTrueTypeFont(fontDesc) && fontDesc.getRuntime();
    }

    static FontDesc getEffectiveFontDesc(FontDesc fontDesc, boolean legacyRuntimeGeneration) {
        FontDesc.Builder builder = fontDesc.toBuilder();

        boolean hasEffects = hasOutline(fontDesc) || hasShadow(fontDesc);
        if (isBitmapFont(fontDesc)) {
            builder.setOutputFormat(FontTextureFormat.TYPE_BITMAP);
            builder.setRenderMode(hasEffects ? FontRenderMode.MODE_MULTI_LAYER : FontRenderMode.MODE_SINGLE_LAYER);
            builder.setRuntime(false);
            builder.setVectorFontMode(VectorFontMode.VECTOR_FONT_MODE_SDF);
            builder.clearSdfMaterial();
            if (fontDesc.getCharacters().isEmpty()) {
                builder.setAllChars(true);
            }
        } else {
            boolean vector = isVectorFont(fontDesc);
            boolean runtime = isTrueTypeFont(fontDesc) &&
                (fontDesc.hasRuntime() ? fontDesc.getRuntime() : vector || legacyRuntimeGeneration);

            builder.setOutputFormat(FontTextureFormat.TYPE_DISTANCE_FIELD);
            builder.setRenderMode(vector || hasEffects ? FontRenderMode.MODE_MULTI_LAYER : FontRenderMode.MODE_SINGLE_LAYER);
            builder.setRuntime(runtime);
            builder.setAntialias(1);

            if (runtime) {
                builder.setAllChars(false);
            }

            if (vector) {
                if (!hasEffects) {
                    builder.setSize(Fontc.VECTOR_REFERENCE_SIZE);
                }
                builder.clearSdfMaterial();
            } else {
                builder.clearSdfMaterial();
            }
        }
        return builder.build();
    }

    private FontDesc getEffectiveFontDesc(FontDesc fontDesc) {
        return getEffectiveFontDesc(fontDesc, legacyRuntimeGeneration());
    }

    private static void validateMaterialMode(IResource input, FontDesc fontDesc, IResource materialResource) throws IOException, CompileExceptionError {
        MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
        TextFormat.merge(new String(materialResource.getContent(), StandardCharsets.UTF_8), materialBuilder);
        MaterialDesc materialDesc = materialBuilder.build();
        boolean hasVectorSampler = materialDesc.getSamplersList().stream().anyMatch(sampler ->
            sampler.getName().equals("curve_texture") || sampler.getName().equals("curve_texture_packed"));

        boolean hasCurveSampler = materialDesc.getSamplersList().stream().anyMatch(sampler -> sampler.getName().equals("curve_texture"));
        boolean hasBandSampler = materialDesc.getSamplersList().stream().anyMatch(sampler -> sampler.getName().equals("band_texture"));
        if (isVectorFont(fontDesc) && (!hasCurveSampler || !hasBandSampler)) {
            throw new CompileExceptionError(input, 0, "Vector font mode requires curve_texture and band_texture samplers");
        }
        if (!isVectorFont(fontDesc) && hasVectorSampler) {
            throw new CompileExceptionError(input, 0, "SDF font mode does not support vector curve texture samplers");
        }
    }

    static FontDesc withInferredBitmapSize(FontDesc fontDesc, IResource fontResource) throws IOException, CompileExceptionError {
        if (!isBitmapFont(fontDesc)) {
            return fontDesc;
        }

        BMFont bitmapFont = new BMFont();
        try (BufferedInputStream stream = new BufferedInputStream(new ByteArrayInputStream(fontResource.getContent()))) {
            bitmapFont.parse(stream);
        } catch (BMFontFormatException e) {
            throw new CompileExceptionError(fontResource, 0, e.getMessage());
        }

        int size = Math.round(bitmapFont.size);
        if (size <= 0) {
            throw new CompileExceptionError(fontResource, 0, "BMFont info.size must be positive");
        }
        return fontDesc.toBuilder().setSize(size).build();
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        FontDesc.Builder builder = getSrcBuilder(input);
        FontDesc fontDesc = getEffectiveFontDesc(builder.build());
        if (isVectorFont(fontDesc) && (hasOutline(fontDesc) || hasShadow(fontDesc)) && fontDesc.getSize() == 0) {
            throw new CompileExceptionError(input, 0, "Vector fonts with outline or shadow effects require a positive Size in the .font resource.");
        }
        IResource fontResource = input.getResource(fontDesc.getFont());
        fontDesc = withInferredBitmapSize(fontDesc, fontResource);
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
            Class<? extends Builder> fontBuilderClass = project.getBuilderFromExtension(fontResource);
            subTask = createSubTask(fontResource, fontBuilderClass, taskBuilder);
        }
        else
        {
            // input(2)
            taskBuilder.addInput(fontResource);
            // input(3)
            subTask = createSubTask(input, GlyphBankBuilder.class, taskBuilder);
        }

        if (!fontDesc.getSdfMaterial().isEmpty()) {
            createSubTask(fontDesc.getSdfMaterial(), "SDF material", taskBuilder);
        }

        Task task = taskBuilder.build();
        subTask.setProductOf(task);
        return task;
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        FontDesc.Builder builder = getSrcBuilder(task.firstInput());
        FontDesc fontDesc = getEffectiveFontDesc(builder.build());
        IResource fontResource = task.firstInput().getResource(fontDesc.getFont());
        fontDesc = withInferredBitmapSize(fontDesc, fontResource);
        FontMap.Builder fontMapBuilder = FontMap.newBuilder();

        BuilderUtil.checkResource(this.project, task.input(1), "material", fontDesc.getMaterial());
        validateMaterialMode(task.firstInput(), fontDesc, task.firstInput().getResource(fontDesc.getMaterial()));
        if (useRuntimeGeneration(fontDesc))
        {
            BuilderUtil.checkResource(this.project, task.firstInput(), "font", fontDesc.getFont());
            // leave glyphbank field empty, as we use that to check at runtime (to toggle runtime generation or not)
            fontMapBuilder.setFont(fontDesc.getFont()); // Keep the suffix as-is (i.e. ".ttf" or ".otf")
        }
        else
        {
            String glyphBankPath   = BuilderUtil.getRelativePath(this.project, task.input(3));
            fontMapBuilder.setGlyphBank(glyphBankPath);
        }

        fontMapBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(fontDesc.getMaterial(), ".material", ".materialc"));
        if (!fontDesc.getSdfMaterial().isEmpty()) {
            fontMapBuilder.setSdfMaterial(ResourceUtil.minifyPathAndReplaceExt(fontDesc.getSdfMaterial(), ".material", ".materialc"));
        }
        if (fontDesc.getAllChars())
        {
            fontMapBuilder.setAllChars(true); // 0x000000 - 0x10FFFF
        }
        else if (useRuntimeGeneration(fontDesc))
        {
            fontMapBuilder.setCharacters(fontDesc.getCharacters());
        }

        try {
            fontMapBuilder.addAllStyles(FontStyles.compileStyles(fontDesc));
        } catch (IllegalArgumentException error) {
            throw new CompileExceptionError(task.firstInput(), 0, error.getMessage(), error);
        }
        fontMapBuilder.setSize(fontDesc.getSize());
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

        fontMapBuilder.setVectorBitmapEffects(isVectorFont(fontDesc));
        fontMapBuilder.setOutputFormat(fontDesc.getOutputFormat());
        fontMapBuilder.setRenderMode(fontDesc.getRenderMode());

        task.output(0).setContent(fontMapBuilder.build().toByteArray());
    }
}
