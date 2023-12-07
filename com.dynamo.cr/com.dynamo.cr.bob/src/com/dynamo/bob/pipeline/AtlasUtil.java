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

package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimIterator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasAnimation;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;
import com.dynamo.gamesys.proto.Tile.Playback;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;
import com.dynamo.proto.DdfMath.Point3;

public class AtlasUtil {
    public static class MappedAnimDesc extends AnimDesc {
        List<String> ids;

        public MappedAnimDesc(String id, List<String> ids, Playback playback, int fps, boolean flipHorizontal,
                boolean flipVertical) {
            super(id, playback, fps, flipHorizontal, flipVertical);
            this.ids = ids;
        }

        public MappedAnimDesc(String id, List<String> ids) {
            super(id, Playback.PLAYBACK_NONE, 0, false, false);
            this.ids = ids;
        }

        public List<String> getIds() {
            return this.ids;
        }
    }

    public static class MappedAnimIterator implements AnimIterator {
        final List<MappedAnimDesc> anims;
        final List<String> imageIds;
        int nextAnimIndex;
        int nextFrameIndex;

        public MappedAnimIterator(List<MappedAnimDesc> anims, List<String> imageIds) {
            this.anims = anims;
            this.imageIds = imageIds;
        }

        @Override
        public AnimDesc nextAnim() {
            if (nextAnimIndex < anims.size()) {
                nextFrameIndex = 0;
                return anims.get(nextAnimIndex++);
            }
            return null;
        }

        @Override
        public Integer nextFrameIndex() {
            MappedAnimDesc anim = anims.get(nextAnimIndex - 1);
            if (nextFrameIndex < anim.getIds().size()) {
                return imageIds.indexOf(anim.getIds().get(nextFrameIndex++));
            }
            return null;
        }

        @Override
        public void rewind() {
            nextAnimIndex = 0;
            nextFrameIndex = 0;
        }
    }

    private static final class AtlasImageSortKey {
        public final String path;
        public final SpriteTrimmingMode mode;
        public AtlasImageSortKey(String path, SpriteTrimmingMode mode) {
            this.path = path;
            this.mode = mode;
        }
        @Override
        public int hashCode() {
            return path.hashCode() + 31 * this.mode.hashCode();
        }
        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            AtlasImageSortKey b = (AtlasImageSortKey)o;
            return this.mode == b.mode && this.path.equals(b.path);
        }
    }

    public static List<AtlasImage> collectImages(Atlas atlas) {
        Map<AtlasImageSortKey, AtlasImage> uniqueImages = new HashMap<AtlasImageSortKey, AtlasImage>();
        List<AtlasImage> images = new ArrayList<AtlasImage>();
        for (AtlasImage image : atlas.getImagesList()) {
            AtlasImageSortKey key = new AtlasImageSortKey(image.getImage(), image.getSpriteTrimMode());
            if (!uniqueImages.containsKey(key)) {
                uniqueImages.put(key, image);
                images.add(image);
            }
        }

        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            for (AtlasImage image : anim.getImagesList() ) {
                AtlasImageSortKey key = new AtlasImageSortKey(image.getImage(), image.getSpriteTrimMode());
                if (!uniqueImages.containsKey(key)) {
                    uniqueImages.put(key, image);
                    images.add(image);
                }
            }
        }
        return images;
    }

    private static List<IResource> toResources(IResource baseResource, List<String> paths) {
        List<IResource> resources = new ArrayList<IResource>(paths.size());
        for (String path : paths) {
            resources.add(baseResource.getResource(path));
        }
        return resources;
    }

    public static List<BufferedImage> loadImages(List<IResource> resources) throws IOException, CompileExceptionError {
        List<BufferedImage> images = new ArrayList<BufferedImage>(resources.size());

        for (IResource resource : resources) {
            BufferedImage image = ImageIO.read(new ByteArrayInputStream(resource.getContent()));
            if (image == null) {
                throw new CompileExceptionError(resource, -1, "Unable to load image " + resource.getPath());
            }
            images.add(image);
        }
        return images;
    }

    public interface PathTransformer {
        String transform(String path);
    }

    private static char PATTERN_SEPARATOR = '=';

    private static String[] getReplaceTokens(String pattern) {
        // "=" -> 0 tokens
        // "abc=" -> 1 token:  ["abc"]
        // "=abc" -> 2 tokens: ["", "abc"]
        // "abc==" -> 1 token: ["abc"]
        // "abc=def=" -> 2 tokens: ["abc", "def"]
        String tokens[] = pattern.split(String.format("%c", PATTERN_SEPARATOR));
        String out[] = new String[]{ "", "" };

        for (int i = 0; i < 2 && i < tokens.length; ++i) {
            out[i] = tokens[i];
        }
        return out;
    }

    public static void validatePattern(String pattern) throws CompileExceptionError  {
        String trimmed = pattern.trim();

        int separatorCount = 0;
        for (int i = 0; i < trimmed.length(); ++i) {
            if (trimmed.charAt(i) == PATTERN_SEPARATOR)
                separatorCount++;
        }

        if (separatorCount == 0) {
            throw new CompileExceptionError(String.format("Rename pattern doesn't contain the '=' separator: '%s'", pattern)); // we cannot match a malformed string
        }
        if (separatorCount > 1) {
            throw new CompileExceptionError(String.format("Rename pattern contains '=' separator more than once: '%s'", pattern)); // we cannot match a malformed string
        }

        if (trimmed.charAt(0) == PATTERN_SEPARATOR) {
            throw new CompileExceptionError(String.format("Rename pattern cannot start with the '=' separator: '%s'", pattern)); // Nothing to replace with
        }
    }

    public static void validatePatterns(String patterns) throws CompileExceptionError  {
        String subPatterns[] = patterns.split(",");
        for (String subPattern : subPatterns) {
            if (subPattern.isEmpty())
                continue;
            validatePattern(subPattern);
        }
    }

    // Pattern is a "=" separated list. We only use the first two tokens
    private static String replaceString(String pattern, String s) throws CompileExceptionError  {
        String tokens[] = getReplaceTokens(pattern);
        return s.replace(tokens[0], tokens[1]);
    }

    // Pattern is a "," separated list list of subpatterns.
    public static String replaceStrings(String patterns, String s) throws CompileExceptionError  {
        if (s.isEmpty())
            return s;
        String subPatterns[] = patterns.split(",");
        for (String subPattern : subPatterns) {
            if (subPattern.isEmpty())
                continue;

            validatePattern(subPattern);
            s = replaceString(subPattern, s);
        }
        return s;
    }

    private static String pathToId(String path) throws CompileExceptionError {
        return FilenameUtils.removeExtension(FilenameUtils.getName(path));
    }

    // These are for the Atlas animation (i.e. not for the single frames)
    private static List<MappedAnimDesc> createAnimDescs(Atlas atlas, PathTransformer transformer) throws CompileExceptionError {
        List<MappedAnimDesc> animDescs = new ArrayList<MappedAnimDesc>(atlas.getAnimationsCount() + atlas.getImagesCount());
        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            List<String> frameIds = new ArrayList<String>();
            for (AtlasImage image : anim.getImagesList()) {
                frameIds.add(transformer.transform(image.getImage()));
            }

            animDescs.add(new MappedAnimDesc(anim.getId(), frameIds, anim.getPlayback(), anim.getFps(),
                    anim.getFlipHorizontal() != 0, anim.getFlipVertical() != 0));
        }
        for (AtlasImage image : atlas.getImagesList()) {
            MappedAnimDesc animDesc = new MappedAnimDesc(transformer.transform(image.getImage()), Collections.singletonList(transformer.transform(image.getImage())));
            animDescs.add(animDesc);
        }

        return animDescs;
    }

    public static TextureSetResult generateTextureSet(final Project project, IResource atlasResource) throws IOException, CompileExceptionError {
        TimeProfiler.start("generateTextureSet");
        Atlas.Builder builder = Atlas.newBuilder();
        ProtoUtil.merge(atlasResource, builder);
        Atlas atlas = builder.build();

        List<AtlasImage> atlasImages = collectImages(atlas);
        List<String> imageResourcePaths = new ArrayList<String>();
        List<SpriteTrimmingMode> imageTrimModes = new ArrayList<>();
        for (AtlasImage image : atlasImages) {
            imageResourcePaths.add(image.getImage());
            imageTrimModes.add(image.getSpriteTrimMode());
        }
        List<IResource> imageResources = toResources(atlasResource, imageResourcePaths);
        List<BufferedImage> images = AtlasUtil.loadImages(imageResources);

        try {
            validatePatterns(atlas.getRenamePatterns());
        } catch(CompileExceptionError e) {
            throw new CompileExceptionError(atlasResource, -1, e.getMessage());
        }

        // The transformer converts paths to basename
        // It is not allowed to have duplicate names
        final String renamePatterns = atlas.getRenamePatterns();
        PathTransformer transformer = new PathTransformer() {
            @Override
            public String transform(String path) {
                String resourcePath = project.getResource(path).getPath();
                String baseName     = FilenameUtils.getBaseName(resourcePath);

                try {
                    return replaceStrings(renamePatterns, baseName);
                } catch (CompileExceptionError e) {
                    System.err.printf("AtlasUtil: Error transforming path: %s\n", e.getMessage());
                }
                return path;
            }
        };

        // Make sure we don't accidentally create a duplicate due to the pattern renaming

        List<String> imageNames = new ArrayList<String>();
        Set<String> uniqueImageNames = new HashSet<String>();
        int imageCount = imageResourcePaths.size();
        for (int i = 0; i < imageCount; ++i) {

            String imageName = transformer.transform(imageResourcePaths.get(i));
            if (uniqueImageNames.contains(imageName)) {
                String message = String.format("Image name '%s' is not unique. Created from path '%s' and rename patterns '%s'", imageName, imageResourcePaths.get(i), renamePatterns);
                throw new CompileExceptionError(atlasResource, -1, message);
            }

            uniqueImageNames.add(imageName);
            imageNames.add(imageName);
        }


        List<MappedAnimDesc> animDescs = createAnimDescs(atlas, transformer);;
        MappedAnimIterator iterator = new MappedAnimIterator(animDescs, imageNames);
        try {
            TextureSetResult result = TextureSetGenerator.generate(images, imageTrimModes, imageNames, iterator,
                Math.max(0, atlas.getMargin()),
                Math.max(0, atlas.getInnerPadding()),
                Math.max(0, atlas.getExtrudeBorders()),
                true, false, null,
                atlas.getMaxPageWidth(), atlas.getMaxPageHeight());

            TimeProfiler.stop();
            return result;
        }
        catch (java.lang.NegativeArraySizeException e) {
            String message = String.format("The generated texture for resource '%s' is too large.", atlasResource.getPath());
            throw new CompileExceptionError(message, e);
        }
    }

    // For tests
    private static List<BufferedImage> loadImagesFromPaths(List<String> resourcePaths) throws IOException, CompileExceptionError {
        List<BufferedImage> images = new ArrayList<BufferedImage>(resourcePaths.size());

        for (String path : resourcePaths) {

            BufferedImage image = ImageIO.read(new FileInputStream(path));

            if (image == null) {
                throw new CompileExceptionError("Unable to load image from path: " + path);
            }
            images.add(image);
        }
        return images;
    }

    // For unit tests only
    public static TextureSetResult generateTextureSet(Atlas atlas, PathTransformer pathTransformer) throws IOException, CompileExceptionError {
        List<AtlasImage> atlasImages = collectImages(atlas);
        List<String> imagePaths = new ArrayList<String>();
        List<String> imageNames = new ArrayList<String>();
        List<SpriteTrimmingMode> imageTrimModes = new ArrayList<>();

        final String renamePatterns = atlas.getRenamePatterns();
        AtlasUtil.PathTransformer nameTransformer = new AtlasUtil.PathTransformer() {
            @Override
            public String transform(String path) {
                String baseName = FilenameUtils.getBaseName(path);

                try {
                    return AtlasUtil.replaceStrings(renamePatterns, baseName);
                } catch (CompileExceptionError e) {
                    System.err.printf("AtlasUtil: Error transforming path: %s\n", e.getMessage());
                }
                return path;
            }
        };

        for (AtlasImage image : atlasImages) {
            imagePaths.add(image.getImage());
            imageNames.add(nameTransformer.transform(image.getImage()));
            imageTrimModes.add(image.getSpriteTrimMode());
        }

        int imagePathCount = imagePaths.size();
        for (int i = 0; i < imagePathCount; ++i) {
            imagePaths.set(i, pathTransformer.transform(imagePaths.get(i)));
        }

        List<BufferedImage> imageDatas = loadImagesFromPaths(imagePaths);

        List<MappedAnimDesc> animDescs = createAnimDescs(atlas, nameTransformer);
        MappedAnimIterator iterator = new MappedAnimIterator(animDescs, imageNames);
        try {
            TextureSetResult result = TextureSetGenerator.generate(imageDatas, imageTrimModes, imageNames, iterator,
                Math.max(0, atlas.getMargin()),
                Math.max(0, atlas.getInnerPadding()),
                Math.max(0, atlas.getExtrudeBorders()),
                true, false, null,
                atlas.getMaxPageWidth(), atlas.getMaxPageHeight());

            return result;
        }
        catch (java.lang.NegativeArraySizeException e) {
            throw new CompileExceptionError("The generated texture is too large.", e);
        }
    }
}
