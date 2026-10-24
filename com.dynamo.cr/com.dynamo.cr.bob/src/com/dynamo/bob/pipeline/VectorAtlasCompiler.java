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

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.slug.SlugCompiler;
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetLayout;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSetMode;
import com.dynamo.gamesys.proto.TextureSetProto.VectorAtlas;
import com.dynamo.gamesys.proto.TextureSetProto.VectorImage;
import com.dynamo.gamesys.proto.Tile.Playback;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;
import com.google.protobuf.ByteString;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.apache.commons.io.FilenameUtils;

/** Shared by Bob and editor: no bitmap allocation, packing, or texture profiles. */
public final class VectorAtlasCompiler {
    private VectorAtlasCompiler() {}

    private static AtlasUtil.PathTransformer imageNames(Atlas atlas) throws CompileExceptionError {
        AtlasUtil.validatePatterns(atlas.getRenamePatterns());
        return path -> {
            try {
                return AtlasUtil.replaceStrings(atlas.getRenamePatterns(), FilenameUtils.getBaseName(path));
            } catch (CompileExceptionError e) {
                throw new IllegalArgumentException(e.getMessage(), e);
            }
        };
    }

    public static TextureSetGenerator.TextureSetResult compile(Atlas atlas, Map<String, byte[]> sources) throws IOException, CompileExceptionError {
        if (!AtlasUtil.isVectorAtlas(atlas)) throw new CompileExceptionError("Expected an SVG atlas");
        AtlasUtil.PathTransformer names = imageNames(atlas);
        List<AtlasImage> images = AtlasUtil.collectImages(null, atlas, names);
        Map<String, Integer> indices = new LinkedHashMap<>();
        List<SvgImporter.Vector> parsed = new ArrayList<>();
        List<SlugCompiler.Image> geometry = new ArrayList<>();
        for (AtlasImage image : images) {
            if (image.getSpriteTrimMode() != SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF)
                throw new CompileExceptionError("Sprite trimming is unsupported for SVG image '" + image.getImage() + "'. Set Sprite Trim Mode to Off.");
            if (!indices.containsKey(image.getImage())) {
                byte[] bytes = sources.get(image.getImage());
                if (bytes == null) throw new IOException("Missing SVG image: " + image.getImage());
                try {
                    indices.put(image.getImage(), geometry.size());
                    SvgImporter.Vector imageData = SvgImporter.parseVector(bytes);
                    parsed.add(imageData);
                    geometry.add(SvgImporter.toVectorImage(imageData.image()));
                } catch (IOException e) {
                    throw new IOException(image.getImage() + ": " + e.getMessage(), e);
                }
            }
        }
        SlugCompiler.Atlas data;
        try {
            data = SlugCompiler.compileAtlas(geometry.toArray(SlugCompiler.Image[]::new));
        } catch (IllegalArgumentException | IllegalStateException e) {
            throw new CompileExceptionError(e.getMessage());
        }
        int last = data.imagePaints()[parsed.size() - 1];
        ByteBuffer nativePaints = data.paints().pixels.duplicate().order(ByteOrder.LITTLE_ENDIAN);
        int usedPaints = last + 1 + (int)nativePaints.getFloat(last * 16) * 6;
        SvgAnimationData animationData = new SvgAnimationData(nativePaints, usedPaints);
        for (var entry : indices.entrySet()) {
            int index = entry.getValue();
            try { animationData.append(parsed.get(index).animation(), data.imagePaints()[index]); }
            catch (IOException e) { throw new IOException(entry.getKey() + ": " + e.getMessage(), e); }
        }
        VectorAtlas.Builder payload = VectorAtlas.newBuilder()
                .setCurves(ByteString.copyFrom(data.curves().pixels.duplicate()))
                .setBands(ByteString.copyFrom(data.bands().pixels.duplicate()))
                .setPaints(ByteString.copyFrom(animationData.finish()));
        List<TextureSetLayout.Rect> rects = new ArrayList<>();
        // A deterministic contact sheet is used only for the atlas editor scene.
        // Runtime sprites sample their own viewport directly from the paint data.
        int columns = Math.max(1, (int)Math.ceil(Math.sqrt(images.size())));
        int cellWidth = 1, cellHeight = 1;
        for (SlugCompiler.Image image : geometry) {
            cellWidth = Math.max(cellWidth, (int)Math.ceil(image.width()));
            cellHeight = Math.max(cellHeight, (int)Math.ceil(image.height()));
        }
        for (int i = 0; i < images.size(); ++i) {
            AtlasImage source = images.get(i);
            int index = indices.get(source.getImage());
            SlugCompiler.Image image = geometry.get(index);
            int w = (int)Math.ceil(image.width()), h = (int)Math.ceil(image.height());
            TextureSetLayout.Rect rect = new TextureSetLayout.Rect(names.transform(source.getImage()), i, 0,
                    (i % columns) * cellWidth, (i / columns) * cellHeight, w, h, false);
            rect.setPivot(new TextureSetLayout.Point(source.getPivotX() * w, (1.0f - source.getPivotY()) * h));
            rects.add(rect);
            payload.addImages(VectorImage.newBuilder().setWidth(image.width()).setHeight(image.height()).setPaint(data.imagePaints()[index])
                    .setDuration((float)parsed.get(index).animation().duration()).setLoop(parsed.get(index).animation().loop()));
        }
        TextureSetLayout.Layout layout = new TextureSetLayout.Layout(columns * cellWidth,
                ((images.size() + columns - 1) / columns) * cellHeight, rects);
        return createTextureSet(atlas, List.of(layout), payload.build());
    }

    /** Rebuilds animation metadata without recompiling geometry, for editor animation edits. */
    public static TextureSetGenerator.TextureSetResult createTextureSet(Atlas atlas, List<TextureSetLayout.Layout> layouts, VectorAtlas payload) throws CompileExceptionError {
        AtlasUtil.PathTransformer names = imageNames(atlas);
        List<AtlasImage> images = AtlasUtil.collectImages(null, atlas, names);
        TextureSetGenerator.TextureSetResult result = TextureSetGenerator.createTextureSet(layouts,
                AtlasUtil.createAnimIterator(atlas, images, names));
        for (int i = 0; i < images.size(); ++i)
            result.builder.setImageNameHashes(i, MurmurHash.hash64(names.transform(images.get(i).getImage())));
        Set<String> implicit = new HashSet<>();
        for (AtlasImage image : atlas.getImagesList()) implicit.add(names.transform(image.getImage()));
        for (var animation : result.builder.getAnimationsBuilderList()) {
            boolean single = implicit.contains(animation.getId());
            if (single) {
                VectorImage image = payload.getImages(result.builder.getFrameIndices(animation.getStart()));
                if (image.getDuration() > 0) animation.setFps(60).setPlayback(image.getLoop() ? Playback.PLAYBACK_LOOP_FORWARD : Playback.PLAYBACK_ONCE_FORWARD);
            }
            boolean animated = false;
            float duration = 0;
            for (int frame = animation.getStart(); frame < animation.getEnd(); ++frame) {
                VectorImage image = payload.getImages(result.builder.getFrameIndices(frame));
                float seconds = image.getDuration() > 0 ? image.getDuration() * (single ? 1 : 60.0f / Math.max(1, animation.getFps())) : 1.0f / Math.max(1, animation.getFps());
                animation.addFrameDurations(seconds);
                duration += seconds;
                animated |= image.getDuration() > 0;
            }
            if (animated) animation.setDuration(duration); else animation.clearFrameDurations();
        }
        // Geometry carries pivots; SVG coverage always uses an untrimmed quad.
        for (var g : result.builder.getGeometriesBuilderList()) {
            g.setTrimMode(SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF);
            g.addAllVertices(List.of(-.5f, -.5f, -.5f, .5f, .5f, .5f, .5f, -.5f));
            g.addAllIndices(List.of(0, 3, 2, 0, 2, 1));
        }
        result.builder.setUseGeometries(0).setTexture("").setMode(TextureSetMode.TEXTURE_SET_MODE_VECTOR).setVector(payload);
        return result;
    }
}
