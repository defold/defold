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

package com.dynamo.bob.util;

import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.stream.Collectors;

import javax.imageio.ImageIO;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.pipeline.TextureGenerator;
import com.dynamo.bob.pipeline.TextureGeneratorException;
import com.dynamo.bob.pipeline.TextureProfilesBuilder;
import com.dynamo.graphics.proto.Graphics.PathSettings;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;
import com.google.protobuf.InvalidProtocolBufferException;

import static com.dynamo.bob.util.MiscUtil.concatenateArrays;

public class TextureUtil {

    private static final int MAX_IMAGE_DATA_ALIGNMENT = 4;

    static {
        TimeProfiler.start("ImageIO.setUseCache");
        ImageIO.setUseCache(false);
        TimeProfiler.stop();
    }

    public static int closestPOT(int i) {
        int nextPow2 = 1;
        while (nextPow2 < i) {
            nextPow2 *= 2;
        }
        int prevPower2 = Math.max(1, nextPow2 / 2);

        int nextDistance = nextPow2 - i;
        int prevDistance = i - prevPower2;

        if (nextDistance <= prevDistance)
            return nextPow2;
        else
            return prevPower2;
    }

    /**
     * Util method to get the image type from an image, or best guess when it's lacking (0).
     *
     * The current java version fails for new BufferedImage(..., 0), which is 0 as image type
     */
    public static int getImageType(BufferedImage image) {
        int type = image.getType();
        if (type == 0) {
            switch (image.getColorModel().getNumColorComponents()) {
            case 3:
                if(image.getColorModel().getComponentSize(0) < 8) {
                    type = BufferedImage.TYPE_USHORT_565_RGB;
                } else {
                    if(image.getColorModel().hasAlpha()) {
                        type = BufferedImage.TYPE_4BYTE_ABGR;
                    } else {
                        type = BufferedImage.TYPE_3BYTE_BGR;
                    }
                }
                break;
            case 1:
                type = BufferedImage.TYPE_BYTE_GRAY;
                break;
            default:
                type = BufferedImage.TYPE_4BYTE_ABGR;
                break;
            }
        }
        return type;
    }

    public static List<BufferedImage> createPaddedImages(List<BufferedImage> srcImages, int paddingAmount, Color paddingColour) {
        List<BufferedImage> result = srcImages;
        if (0 < paddingAmount) {
            result = new ArrayList<BufferedImage>(srcImages.size());
            for (BufferedImage srcImage : srcImages) {
                result.add(createPaddedImage(srcImage, paddingAmount, paddingColour));
            }
        }
        return result;
    }

    public static BufferedImage createPaddedImage(BufferedImage src, int paddingAmount, Color paddingColour) {
        int origWidth = src.getWidth();
        int origHeight = src.getHeight();
        int newWidth = origWidth + paddingAmount * 2;
        int newHeight = origHeight + paddingAmount * 2;

        BufferedImage result = new BufferedImage(newWidth, newHeight, BufferedImage.TYPE_4BYTE_ABGR);

        Graphics2D graphics = result.createGraphics();
        graphics.setBackground(paddingColour);
        graphics.clearRect(0, 0, newWidth, newHeight);
        graphics.drawImage(src, paddingAmount, paddingAmount, null);
        graphics.dispose();

        return result;
    }

    public static List<BufferedImage> extrudeBorders(List<BufferedImage> srcImages, int extrudeBorders) {
        List<BufferedImage> result = srcImages;
        if (extrudeBorders > 0) {
            result = new ArrayList<BufferedImage>(srcImages.size());
            for (BufferedImage srcImage : srcImages) {
                result.add(extrudeBorders(srcImage, extrudeBorders));
            }
        }
        return result;
    }

    public static BufferedImage extrudeBorders(BufferedImage src, int extrudeBorders) {
        if (extrudeBorders > 0) {
            src = depalettiseImage(src);

            int origWidth = src.getWidth();
            int origHeight = src.getHeight();
            int newWidth = origWidth + extrudeBorders * 2;
            int newHeight = origHeight + extrudeBorders * 2;
            int type = getImageType(src);

            BufferedImage tgt = new BufferedImage(newWidth, newHeight, type);
            int numComponents = src.getColorModel().getNumComponents();
            int[] srcPixels = new int[origWidth * origHeight * numComponents];
            src.getRaster().getPixels(0, 0, origWidth, origHeight, srcPixels);
            int[] tgtPixels = new int[newWidth * newHeight * numComponents];
            for (int y = 0; y < newHeight; ++y) {
                for (int x = 0; x < newWidth; ++x) {
                    int index = (x + y * newWidth) * numComponents;
                    int sx = Math.min(Math.max(x - extrudeBorders, 0), origWidth-1);
                    int sy = Math.min(Math.max(y - extrudeBorders, 0), origHeight-1);
                    int sindex = (sx + sy * origWidth) * numComponents;
                    for (int i = 0; i < numComponents; ++i) {
                        tgtPixels[index + i] = srcPixels[sindex + i];
                    }
                }
            }
            tgt.getRaster().setPixels(0, 0, newWidth, newHeight, tgtPixels);
            return tgt;
        } else {
            return src;
        }
    }

    public static BufferedImage depalettiseImage(BufferedImage src) {
        BufferedImage result = null;
        if (BufferedImage.TYPE_BYTE_INDEXED == src.getType() || BufferedImage.TYPE_BYTE_BINARY == src.getType() || src.getColorModel().getNumColorComponents() < 3 || src.getColorModel().getComponentSize(0) > 8) {
            int width = src.getWidth();
            int height = src.getHeight();
            Color clear = new Color(255,255,255,0);

            result = new BufferedImage(width, height, BufferedImage.TYPE_4BYTE_ABGR);

            Graphics2D graphics = result.createGraphics();
            graphics.setBackground(clear);
            graphics.clearRect(0, 0, width, height);
            graphics.drawImage(src, 0, 0, null);
            graphics.dispose();
        } else {
            result = src;
        }
        return result;
    }

    /**
     * Util method to get the correct texture profile for a specific resource path.
     * We need a "built" texture profile message, i.e. where path entries has been
     * substituted with their regex counterpart.
     *
     * Returns null if no profile was found, as will be expected by the second parameter
     * to TextureGenerator.generate().
     */
    public static TextureProfile getTextureProfileByPath(TextureProfiles textureProfiles, String path) {

        if (textureProfiles == null)
            return null;

        // Paths are root relative inside Bob, but to the user resource paths look absolute (see paths
        // specified in the game project UI). We need to match against the users expectations:
        path = "/" + path;

        // Look through the path settings if the current input filepath matches,
        // if found, lookup the correct texture profile to use.
        for (PathSettings pathSettings : textureProfiles.getPathSettingsList()) {
            // Path matches are stored as ant-glob
            if (PathUtil.wildcardMatch(path, pathSettings.getPath())) {

                // Find matching profile name (there could be a reference to an non-existent profile):
                for (TextureProfile texProfile : textureProfiles.getProfilesList()) {
                    if (texProfile.getName().toString().equals(pathSettings.getProfile().toString())) {
                        return texProfile;
                    }
                }
                break;
            }
        }

        return null;
    }
    private static String textureProfilesExt = TextureProfilesBuilder.class.getAnnotation(BuilderParams.class).outExt();

    public static TextureProfile getTextureProfileByPath(IResource textureProfiles, String path) throws IOException {
        TextureProfiles.Builder builder = TextureProfiles.newBuilder();
        if (!textureProfiles.exists() || !textureProfiles.getPath().endsWith(textureProfilesExt)) {
            return null;
        }
        builder.mergeFrom(textureProfiles.getContent());
        return getTextureProfileByPath(builder.build(), path);
    }

    public static void addTextureProfileInput(Task.TaskBuilder taskBuilder, Project project) {
        String textureProfilesPath = project.getProjectProperties().getStringValue("graphics", "texture_profiles");
        if (textureProfilesPath != null) {
            String fileName = ResourceUtil.changeExt(textureProfilesPath, textureProfilesExt);
            taskBuilder.addInput(project.getResource(fileName).output());
        }
    }

    private static int getTextureGenerateResultImageDatasIndex(TextureImage textureImage, int alternative, int mipMap) throws TextureGeneratorException {
        int mipMapOffset = 0;
        for (int i=0; i < textureImage.getAlternativesCount(); i++) {
            if (i == alternative) {
                return mipMapOffset + mipMap;
            }
            mipMapOffset += textureImage.getAlternatives(i).getMipMapOffsetCount();
        }
        throw new TextureGeneratorException("Invalid parameters");
    }

    public static TextureGenerator.GenerateResult createCombinedTextureImage(TextureGenerator.GenerateResult[] generateResults, TextureImage.Type type) throws TextureGeneratorException {
        int numTextures = generateResults.length;
        if (numTextures == 0) {
            return null;
        }

        if (type == TextureImage.Type.TYPE_2D)
        {
            numTextures = 1;
        }

        TextureGenerator.GenerateResult resultOut = new TextureGenerator.GenerateResult();
        resultOut.imageDatas = new ArrayList<>();

        TextureImage.Builder combinedImageBuilder = TextureImage.newBuilder(generateResults[0].textureImage);

        for (int i = 0; i < combinedImageBuilder.getAlternativesCount(); i++) {
            TextureImage.Image.Builder alternativeImageBuilder = TextureImage.Image.newBuilder(generateResults[0].textureImage.getAlternatives(i));
            alternativeImageBuilder.clearMipMapSizeCompressed();

            int mipMapCount = alternativeImageBuilder.getMipMapSizeCount();
            int byteSize = 0;

            for (int j = 0; j < mipMapCount; j++) {

                for (int k = 0; k < numTextures; k++) {
                    int mipSize = generateResults[k].textureImage.getAlternatives(i).getMipMapSize(j);
                    alternativeImageBuilder.addMipMapSizeCompressed(mipSize);

                    ArrayList<byte[]> imageMipmapDatas = generateResults[k].imageDatas;
                    int mipImageIndex = getTextureGenerateResultImageDatasIndex(generateResults[k].textureImage, i, j);
                    byte[] mipBytes = imageMipmapDatas.get(mipImageIndex);

                    resultOut.imageDatas.add(mipBytes);
                    byteSize += mipSize;
                }
            }

            alternativeImageBuilder.setDataSize(byteSize);

            for (int j = 0; j < alternativeImageBuilder.getMipMapSizeCount(); j++) {
                alternativeImageBuilder.setMipMapOffset(j, alternativeImageBuilder.getMipMapOffset(j) * numTextures);
            }
            combinedImageBuilder.setAlternatives(i, alternativeImageBuilder);
        }

        combinedImageBuilder.setCount(numTextures);
        combinedImageBuilder.setType(type);

        resultOut.textureImage = combinedImageBuilder.build();
        return resultOut;
    }

    public static BufferedImage Resize(BufferedImage img, int newW, int newH, int type) {
        BufferedImage dimg = new BufferedImage(newW, newH, type);
        Graphics2D g2d = dimg.createGraphics();
        g2d.drawImage(img, 0, 0, img.getWidth(), img.getHeight(), null);
        g2d.dispose();
        return dimg;
    }

    // Called from bob and editor when building texture resources
    public static void writeGenerateResultToOutputStream(TextureGenerator.GenerateResult generateResult, OutputStream stream) throws IOException {
        writeGenerateResultToOutputStream(generateResult, stream, MAX_IMAGE_DATA_ALIGNMENT);
    }

    public static void writeGenerateResultToOutputStream(TextureGenerator.GenerateResult generateResult, OutputStream stream, int imageDataAlignment) throws IOException {
        AlignedGenerateResult alignedGenerateResult = alignImageData(generateResult, imageDataAlignment);
        byte[] header = alignedGenerateResult.textureImage.toByteArray();

        ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        buffer.putInt(header.length);
        byte[] headerSizeInBytes = buffer.array();

        stream.write(headerSizeInBytes);
        stream.write(header);
        for (byte[] imageData : alignedGenerateResult.imageDatas) {
            stream.write(imageData);
        }
        stream.flush();
    }

    public static void writeGenerateResultToResource(TextureGenerator.GenerateResult generateResult, IResource resource) throws IOException {
        writeGenerateResultToResource(generateResult, resource, MAX_IMAGE_DATA_ALIGNMENT);
    }

    public static void writeGenerateResultToResource(TextureGenerator.GenerateResult generateResult, IResource resource, int imageDataAlignment) throws IOException {
        AlignedGenerateResult alignedGenerateResult = alignImageData(generateResult, imageDataAlignment);
        byte[] header = alignedGenerateResult.textureImage.toByteArray();

        ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        buffer.putInt(header.length);
        byte[] headerSizeInBytes = buffer.array();

        resource.setContent(headerSizeInBytes);
        resource.appendContent(header);

        for (byte[] imageData : alignedGenerateResult.imageDatas) {
            resource.appendContent(imageData);
        }
    }

    private static class AlignedGenerateResult {
        final TextureImage textureImage;
        final ArrayList<byte[]> imageDatas;

        AlignedGenerateResult(TextureImage textureImage, ArrayList<byte[]> imageDatas) {
            this.textureImage = textureImage;
            this.imageDatas = imageDatas;
        }
    }

    private static AlignedGenerateResult alignImageData(TextureGenerator.GenerateResult generateResult, int maxAlignment) {
        if (maxAlignment <= 1 || generateResult.imageDatas.isEmpty() || generateResult.textureImage.getAlternativesCount() == 0) {
            return new AlignedGenerateResult(generateResult.textureImage, generateResult.imageDatas);
        }

        int alternativeCount = generateResult.textureImage.getAlternativesCount();
        int[] alternativeAlignments = new int[alternativeCount];
        int[] alternativePaddings = new int[alternativeCount];
        boolean needsAlignment = false;
        for (int i = 0; i < alternativeCount; ++i) {
            alternativeAlignments[i] = Math.min(maxAlignment, getTextureFormatDataAlignment(generateResult.textureImage.getAlternatives(i).getFormat()));
            needsAlignment |= alternativeAlignments[i] > 1;
        }

        if (!needsAlignment) {
            return new AlignedGenerateResult(generateResult.textureImage, generateResult.imageDatas);
        }

        for (int attempt = 0; attempt < alternativeCount * maxAlignment + 1; ++attempt) {
            TextureImage alignedTextureImage = addImagePadding(generateResult.textureImage, alternativePaddings);
            int headerSize = alignedTextureImage.toByteArray().length;
            int alternativeOffset = 0;
            boolean changed = false;

            for (int i = 0; i < alternativeCount; ++i) {
                TextureImage.Image image = generateResult.textureImage.getAlternatives(i);
                int firstMipOffset = image.getMipMapOffsetCount() > 0 ? image.getMipMapOffset(0) : 0;
                int unpaddedImageOffset = Integer.BYTES + headerSize + alternativeOffset + firstMipOffset;
                int alignment = alternativeAlignments[i];
                int padding = alignment <= 1 ? 0 : (alignment - (unpaddedImageOffset % alignment)) % alignment;
                if (alternativePaddings[i] != padding) {
                    alternativePaddings[i] = padding;
                    changed = true;
                }
                alternativeOffset += image.getDataSize() + padding;
            }

            if (!changed) {
                return new AlignedGenerateResult(alignedTextureImage, addImageDataPadding(generateResult, alternativePaddings));
            }
        }

        throw new IllegalStateException(String.format("Unable to align texture image data to %d bytes", maxAlignment));
    }

    private static int getTextureFormatDataAlignment(TextureImage.TextureFormat format) {
        switch (format) {
            case TEXTURE_FORMAT_RGB_16BPP:
            case TEXTURE_FORMAT_RGBA_16BPP:
            case TEXTURE_FORMAT_RGB16F:
            case TEXTURE_FORMAT_RGBA16F:
            case TEXTURE_FORMAT_R16F:
            case TEXTURE_FORMAT_RG16F:
            case TEXTURE_FORMAT_R16_UNORM:
                return 2;
            case TEXTURE_FORMAT_RGB32F:
            case TEXTURE_FORMAT_RGBA32F:
            case TEXTURE_FORMAT_R32F:
            case TEXTURE_FORMAT_RG32F:
                return 4;
            default:
                return 1;
        }
    }

    private static TextureImage addImagePadding(TextureImage textureImage, int[] alternativePaddings) {
        TextureImage.Builder textureImageBuilder = textureImage.toBuilder();
        for (int i = 0; i < alternativePaddings.length; ++i) {
            textureImageBuilder.setAlternatives(i, addImagePadding(textureImage.getAlternatives(i), alternativePaddings[i]));
        }
        return textureImageBuilder.build();
    }

    private static TextureImage.Image addImagePadding(TextureImage.Image image, int padding) {
        if (padding == 0) {
            return image;
        }

        // The padding is part of the alternative's data range; mip offsets skip over it at runtime.
        TextureImage.Image.Builder imageBuilder = image.toBuilder();
        imageBuilder.setDataSize(image.getDataSize() + padding);
        for (int i = 0; i < image.getMipMapOffsetCount(); ++i) {
            imageBuilder.setMipMapOffset(i, image.getMipMapOffset(i) + padding);
        }
        return imageBuilder.build();
    }

    private static ArrayList<byte[]> addImageDataPadding(TextureGenerator.GenerateResult generateResult, int[] alternativePaddings) {
        ArrayList<byte[]> imageDatas = new ArrayList<>();
        int imageDataIndex = 0;

        for (int i = 0; i < generateResult.textureImage.getAlternativesCount(); ++i) {
            int padding = alternativePaddings[i];
            if (padding > 0) {
                imageDatas.add(new byte[padding]);
            }

            int remainingDataSize = generateResult.textureImage.getAlternatives(i).getDataSize();
            while (remainingDataSize > 0) {
                if (imageDataIndex >= generateResult.imageDatas.size()) {
                    throw new IllegalStateException("Texture image data is smaller than the texture metadata describes");
                }

                byte[] imageData = generateResult.imageDatas.get(imageDataIndex++);
                if (imageData.length > remainingDataSize) {
                    throw new IllegalStateException("Texture image data spans multiple texture alternatives");
                }

                imageDatas.add(imageData);
                remainingDataSize -= imageData.length;
            }
        }

        if (imageDataIndex != generateResult.imageDatas.size()) {
            throw new IllegalStateException("Texture image data is larger than the texture metadata describes");
        }
        return imageDatas;
    }

    public static TextureImage textureResourceBytesToTextureImage(byte[] content) throws InvalidProtocolBufferException {
        // Read the header size (first 4 bytes)
        ByteBuffer buffer = ByteBuffer.wrap(content);
        ByteOrder currentOrder = buffer.order();
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        int headerSize = buffer.getInt();
        buffer.order(currentOrder);

        byte[] textureImage = new byte[headerSize];
        System.arraycopy(content, 4, textureImage, 0, headerSize);

        return TextureImage.parseFrom(textureImage);
    }

    // Public api
    public static TextureGenerator.GenerateResult createMultiPageTexture(List<BufferedImage> images, TextureImage.Type textureType, TextureProfile texProfile, boolean compress) throws TextureGeneratorException {
        TextureGenerator.GenerateResult[] generateResults = new TextureGenerator.GenerateResult[images.size()];

        // Since external tools may provide pages of varying sizes, we need to make sure
        // our output is of the correct dimensions (as we're using a texture array)
        int maxWidth = 0;
        int maxHeight = 0;
        int firstType = -1;

        for (BufferedImage image : images) {
            maxWidth = Math.max(maxWidth, image.getWidth());
            maxHeight = Math.max(maxHeight, image.getHeight());

            if (firstType < 0)
                firstType = image.getType();
        }

        for (int i = 0; i < images.size(); i++)
        {
            try {
                BufferedImage image = images.get(i);
                // Make sure they're the same dimension and type!
                if (image.getWidth() != maxWidth ||
                    image.getHeight() != maxHeight ||
                    image.getType() != firstType)
                {
                    image = Resize(image, maxWidth, maxHeight, firstType);
                }

                generateResults[i] = TextureGenerator.generate(image, texProfile, compress);
            } catch (IOException e) {
                throw new TextureGeneratorException(e.getMessage());
            }
        }

        return TextureUtil.createCombinedTextureImage(generateResults, textureType);
    }

    // Public api
    public static List<BufferedImage> loadImages(List<IResource> resources) throws IOException, CompileExceptionError {
        try {
            return resources.parallelStream()
                    .map(resource -> {
                        try {
                            BufferedImage image = ImageIO.read(new ByteArrayInputStream(resource.getContent()));
                            if (image == null) {
                                throw new CompileExceptionError(resource, -1, "Unable to load image " + resource.getPath());
                            }
                            return image;
                        } catch (IOException e) {
                            throw new RuntimeException(new CompileExceptionError(resource, -1, "Unable to load image " + resource.getPath(), e));
                        } catch (CompileExceptionError e) {
                            throw new RuntimeException(e);
                        }
                    })
                    .collect(Collectors.toList());
        } catch (RuntimeException e) {
            if (e.getCause() instanceof CompileExceptionError) {
                throw (CompileExceptionError) e.getCause();
            }
            throw e;
        }
    }

    static HashMap<String, String> ATLAS_FILE_TYPES = new HashMap<>();
    static {
        ATLAS_FILE_TYPES.put(".atlas", ".a.texturesetc");
        ATLAS_FILE_TYPES.put(".tileset", ".t.texturesetc");
        ATLAS_FILE_TYPES.put(".tilesource", ".t.texturesetc");
    }

    public static Map<String, String> getAtlasFileTypes() {
        return ATLAS_FILE_TYPES;
    }

    public static boolean isAtlasFileType(String srcSuffix) {
        return ATLAS_FILE_TYPES.containsKey(srcSuffix);
    }

    // Public SDK api for extensions!
    public static void registerAtlasFileType(String srcSuffix) {
        ATLAS_FILE_TYPES.put(srcSuffix, ATLAS_FILE_TYPES.get(".atlas"));
    }
}
