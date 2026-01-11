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

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.HashSet;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimIterator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasAnimation;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;
import com.dynamo.gamesys.proto.Tile.Playback;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;

public class AtlasUtil {
    public static class MappedAnimDesc extends AnimDesc {
        private List<String> paths;
        private List<String> ids;
        private boolean singleFrame;

        public MappedAnimDesc(String id, List<String> paths, List<String> ids, Playback playback, int fps, boolean flipHorizontal, boolean flipVertical) {
            super(id, playback, fps, flipHorizontal, flipVertical);
            this.paths = paths;
            this.ids = ids;
            this.singleFrame = false;
        }

        public MappedAnimDesc(String id, List<String> paths, List<String> ids) {
            super(id, Playback.PLAYBACK_NONE, 0, false, false);
            this.paths = paths;
            this.ids = ids;
            this.singleFrame = true; // This animation is from a single frame animation
        }

        public List<String> getIds() { // For testing
            return this.ids;
        }

        public List<String> getPaths() {
            return this.paths;
        }
    }

    public static class MappedAnimIterator implements AnimIterator {
        final List<MappedAnimDesc> anims;
        final List<String> imagePaths;
        int nextAnimIndex;
        int nextFrameIndex;

        public MappedAnimIterator(List<MappedAnimDesc> anims, List<String> imagePaths) {
            this.anims = anims;
            this.imagePaths = imagePaths;
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
        public Integer nextFrameIndex() { // Return the global index of the image that the frame is using
            MappedAnimDesc anim = anims.get(nextAnimIndex - 1);
            if (nextFrameIndex < anim.getPaths().size()) {
                return imagePaths.indexOf(anim.getPaths().get(nextFrameIndex++));
            }
            return null;
        }

        @Override
        public String getFrameId() {
            MappedAnimDesc anim = anims.get(nextAnimIndex - 1);
            String prefix = "";
            if (!anim.singleFrame)
                prefix = anim.getId() + "/";
            return prefix + anim.getIds().get(nextFrameIndex-1);
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

    private static void sortImages(List<AtlasImage> images) {
        images.sort(new Comparator<AtlasImage>() {
            @Override
            public int compare(AtlasImage img1, AtlasImage img2) {
                int nameComparison = img1.getImage().compareTo(img2.getImage());
                if (nameComparison == 0) {
                    return Integer.compare(img1.getSpriteTrimMode().getNumber(), img2.getSpriteTrimMode().getNumber());
                }
                return nameComparison;
            }
        });
    }

    public static List<AtlasImage> collectImages(IResource atlasResource, Atlas atlas, PathTransformer transformer) throws CompileExceptionError {
        Map<AtlasImageSortKey, AtlasImage> uniqueImages = new HashMap<>();
        HashSet<String> uniqueNames = new HashSet<>();
        List<AtlasImage> images = new ArrayList<AtlasImage>();
        for (AtlasImage image : atlas.getImagesList()) {

            AtlasImageSortKey key = new AtlasImageSortKey(image.getImage(), image.getSpriteTrimMode());
            if (!uniqueImages.containsKey(key)) {
                uniqueImages.put(key, image);
                images.add(image);
            }

            //System.out.printf("R: %s -> %s\n", imageResourcePaths.get(i), imageName);

            // We'll check that the explicitly added top level (single frame animations) won't collide
            String imageName = transformer.transform(image.getImage());
            if (uniqueNames.contains(imageName))
            {
                String message = String.format("Image name '%s' is not unique. Created from path '%s'", imageName, image.getImage());
                throw new CompileExceptionError(atlasResource, -1, message);
            }
            uniqueNames.add(imageName);
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
        sortImages(images);
        return images;
    }

    private static List<IResource> toResources(IResource baseResource, List<String> paths) {
        List<IResource> resources = new ArrayList<IResource>(paths.size());
        for (String path : paths) {
            resources.add(baseResource.getResource(path));
        }
        return resources;
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

    // These are for the Atlas animation
    private static List<MappedAnimDesc> createAnimDescs(Atlas atlas, PathTransformer transformer) throws CompileExceptionError {
        List<MappedAnimDesc> animDescs = new ArrayList<MappedAnimDesc>(atlas.getAnimationsCount() + atlas.getImagesCount());
        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            List<String> framePaths = new ArrayList<String>();
            List<String> frameIds = new ArrayList<String>();

            // For accurate frame index lookup, we still need the paths
            // Otherwise we couldn't separate two such animations:
            //   anim1: a/1.png, a/2.png ...
            //   anim2: b/1.png, b/2.png ...
            for (AtlasImage image : anim.getImagesList()) {
                framePaths.add(image.getImage());
                frameIds.add(transformer.transform(image.getImage()));
            }

            animDescs.add(new MappedAnimDesc(anim.getId(), framePaths, frameIds, anim.getPlayback(), anim.getFps(), anim.getFlipHorizontal() != 0, anim.getFlipVertical() != 0));
        }
        List<AtlasImage> images = new ArrayList<>(atlas.getImagesList());
        sortImages(images);
        for (AtlasImage image : images) {
            String id = transformer.transform(image.getImage());
            animDescs.add(new MappedAnimDesc(id, Collections.singletonList(image.getImage()), Collections.singletonList(id)));
        }

        return animDescs;
    }

    public static PathTransformer createPathTransformer(final Project project, final String renamePatterns) {
        return new PathTransformer() {
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
    }

    public static TextureSetResult generateTextureSet(final Project project, IResource atlasResource, Atlas.Builder builder) throws IOException, CompileExceptionError {
        TimeProfiler.start("generateTextureSet");
        Atlas atlas = builder.build();

        try {
            validatePatterns(atlas.getRenamePatterns());
        } catch(CompileExceptionError e) {
            throw new CompileExceptionError(atlasResource, -1, e.getMessage());
        }

        // The transformer converts paths to basename
        // It is not allowed to have duplicate names
        PathTransformer transformer = createPathTransformer(project, atlas.getRenamePatterns());

        List<AtlasImage> atlasImages = collectImages(atlasResource, atlas, transformer);
        List<String> imageResourcePaths = new ArrayList<String>();
        for (AtlasImage image : atlasImages) {
            imageResourcePaths.add(image.getImage());
        }
        List<IResource> imageResources = toResources(atlasResource, imageResourcePaths);
        List<BufferedImage> images = TextureUtil.loadImages(imageResources);

        List<String> imageNames = new ArrayList<String>();
        int imageCount = imageResourcePaths.size();
        for (int i = 0; i < imageCount; ++i) {

            String imageName = transformer.transform(imageResourcePaths.get(i));
            imageNames.add(imageName);
        }

        List<MappedAnimDesc> animDescs = createAnimDescs(atlas, transformer);
        MappedAnimIterator iterator = new MappedAnimIterator(animDescs, imageResourcePaths);
        try {
            TextureSetResult result = TextureSetGenerator.generate(images, atlasImages, imageNames, iterator,
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

    // For AtlasBuilder,java:main()
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

    // For AtlasBuilder.java:main()
    public static TextureSetResult generateTextureSet(Atlas atlas, PathTransformer pathTransformer) throws IOException, CompileExceptionError {
        List<String> imagePaths = new ArrayList<String>();
        List<String> imageAbsolutePaths = new ArrayList<String>();
        List<String> imageNames = new ArrayList<String>();

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

        List<AtlasImage> atlasImages = collectImages(null, atlas, nameTransformer);

        for (AtlasImage image : atlasImages) {
            imagePaths.add(image.getImage());
            imageNames.add(nameTransformer.transform(image.getImage()));
        }

        for (String path : imagePaths) {
            imageAbsolutePaths.add(pathTransformer.transform(path));
        }

        List<BufferedImage> imageDatas = loadImagesFromPaths(imageAbsolutePaths);

        List<MappedAnimDesc> animDescs = createAnimDescs(atlas, nameTransformer);
        MappedAnimIterator iterator = new MappedAnimIterator(animDescs, imagePaths);
        try {
            TextureSetResult result = TextureSetGenerator.generate(imageDatas, atlasImages, imageNames, iterator,
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
