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

package com.dynamo.bob.util;

import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.awt.Image;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

import javax.imageio.ImageIO;

import com.google.protobuf.ByteString;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.TextureGenerator;
import com.dynamo.bob.pipeline.TextureGeneratorException;
import com.dynamo.graphics.proto.Graphics.PathSettings;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;

public class TextureUtil {
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

    public static TextureImage createCombinedTextureImage(TextureImage[] textures, TextureImage.Type type) throws TextureGeneratorException {
        int numTextures = textures.length;
        if (numTextures == 0) {
            return null;
        }

        if (type == TextureImage.Type.TYPE_2D)
        {
            numTextures = 1;
        }

        TextureImage.Builder combinedImageBuilder = TextureImage.newBuilder(textures[0]);

        for (int i = 0; i < combinedImageBuilder.getAlternativesCount(); i++) {
            TextureImage.Image.Builder alternativeImageBuilder = TextureImage.Image.newBuilder(textures[0].getAlternatives(i));

            alternativeImageBuilder.clearMipMapSizeCompressed();

            try {
                ByteArrayOutputStream os = new ByteArrayOutputStream(1024 * 4);
                for (int j = 0; j < alternativeImageBuilder.getMipMapSizeCount(); j++) {

                    for (int k = 0; k < numTextures; k++) {
                        int mipSize = textures[k].getAlternatives(i).getMipMapSize(j);
                        ByteString data = textures[k].getAlternatives(i).getData();
                        int mipOffset = alternativeImageBuilder.getMipMapOffset(j);
                        alternativeImageBuilder.addMipMapSizeCompressed(mipSize);

                        // Sizes can change between textures (maybe resize only if needed)
                        byte[] buf = new byte[mipSize];
                        data.copyTo(buf, mipOffset, 0, mipSize);
                        os.write(buf);
                    }
                }
                os.flush();
                alternativeImageBuilder.setData(ByteString.copyFrom(os.toByteArray()));

            } catch (IOException e) {
                throw new TextureGeneratorException(e.getMessage());
            }

            for (int j = 0; j < alternativeImageBuilder.getMipMapSizeCount(); j++) {
                alternativeImageBuilder.setMipMapOffset(j, alternativeImageBuilder.getMipMapOffset(j) * numTextures);
            }
            combinedImageBuilder.setAlternatives(i, alternativeImageBuilder);
        }

        combinedImageBuilder.setCount(numTextures);
        combinedImageBuilder.setType(type);
        return combinedImageBuilder.build();
    }

    public static BufferedImage Resize(BufferedImage img, int newW, int newH, int type) {
        Image tmp = img.getScaledInstance(newW, newH, Image.SCALE_SMOOTH);
        BufferedImage dimg = new BufferedImage(newW, newH, type);

        Graphics2D g2d = dimg.createGraphics();
        g2d.drawImage(tmp, 0, 0, null);
        g2d.dispose();

        return dimg;
    }

    // Public api
    public static TextureImage createMultiPageTexture(List<BufferedImage> images, TextureImage.Type textureType, TextureProfile texProfile, boolean compress) throws TextureGeneratorException {
        TextureImage textureImages[] = new TextureImage[images.size()];

        // Since external tools may provide pages of verying sizes, we need to make sure
        // our output is of the correct dimensions (as we're using a texture array)
        int maxWidth = 0;
        int maxHeight = 0;
        int firstType = -1;

        for (int i = 0; i < images.size(); i++)
        {
            BufferedImage image = images.get(i);
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

                textureImages[i] = TextureGenerator.generate(image, texProfile, compress);
            } catch (IOException e) {
                throw new TextureGeneratorException(e.getMessage());
            }
        }
        return TextureUtil.createCombinedTextureImage(textureImages, textureType);
    }

    // Public api
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
