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

package com.dynamo.bob.font;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Composite;
import java.awt.CompositeContext;
import java.awt.Font;
import java.awt.FontFormatException;
import java.awt.FontMetrics;
import java.awt.Graphics2D;
import java.awt.Rectangle;
import java.awt.RenderingHints;
import java.awt.Shape;
import java.awt.Stroke;
import java.awt.font.FontRenderContext;
import java.awt.font.GlyphMetrics;
import java.awt.font.GlyphVector;
import java.awt.geom.AffineTransform;
import java.awt.geom.FlatteningPathIterator;
import java.awt.geom.PathIterator;
import java.awt.geom.Area;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.ConvolveOp;
import java.awt.image.Kernel;
import java.awt.image.Raster;
import java.awt.image.WritableRaster;
import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.TreeSet;
import java.util.Set;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;
import com.sun.jna.Pointer;

import com.dynamo.bob.pipeline.Texc;
import com.dynamo.bob.pipeline.TexcLibraryJni;

import com.dynamo.bob.pipeline.BuilderUtil;
import com.dynamo.bob.pipeline.TextureGeneratorException;

import com.dynamo.bob.util.StringUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.font.BMFont.BMFontFormatException;
import com.dynamo.bob.font.BMFont.Char;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.GlyphBank;
import com.dynamo.render.proto.Font.FontTextureFormat;
import com.dynamo.render.proto.Font.FontRenderMode;
import com.google.protobuf.ByteString;
import com.google.protobuf.TextFormat;
import org.apache.commons.lang3.StringUtils;

class OrderComparator implements Comparator<Fontc.Glyph> {
    @Override
    public int compare(Fontc.Glyph o1, Fontc.Glyph o2) {
        return (Integer.valueOf(o1.index)).compareTo(o2.index);
    }
}

class BlendContext implements CompositeContext {
    @Override
    public void compose(Raster src, Raster dstIn, WritableRaster dstOut) {
        int width = Math.min(src.getWidth(), Math.min(dstIn.getWidth(), dstOut.getWidth()));
        int height = Math.min(src.getHeight(), Math.min(dstIn.getHeight(), dstOut.getHeight()));
        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < height; ++j) {
                float sr = src.getSampleFloat(i, j, 0);
                float sg = src.getSampleFloat(i, j, 1);
                if (sr > 0.0f)
                    dstOut.setSample(i, j, 0, sr);
                else
                    dstOut.setSample(i, j, 0, dstIn.getSample(i, j, 0));
                if (sg > 0.0f)
                    dstOut.setSample(i, j, 1, sg);
                else
                    dstOut.setSample(i, j, 1, dstIn.getSample(i, j, 1));

                dstOut.setSample(i, j, 2, dstIn.getSample(i, j, 2));
            }
        }
    }

    @Override
    public void dispose() {}
}

class BlendComposite implements Composite {
    @Override
    public CompositeContext createContext(ColorModel arg0, ColorModel arg1, RenderingHints arg2) {
        return new BlendContext();
    }
}

public class Fontc {

    public static final char[] ASCII_7BIT;
    static {
        int start = 32;
        int end = 126;
        ASCII_7BIT = new char[end - start + 1];
        for (int i = start; i <= end; i++) {
            ASCII_7BIT[i - start] = (char) i;
        }
    }

    public enum InputFontFormat {
        FORMAT_TRUETYPE,
        FORMAT_BMFONT
    };

    public class Glyph {
        public int           index;
        public int           c;
        public int           width;
        public int           advance;
        public int           leftBearing;
        public int           ascent;
        public int           descent;
        public int           x;
        public int           y;
        public int           cache_entry_offset;
        public int           cache_entry_size;
        public GlyphVector   vector;
        public BufferedImage image;
    };

    static final float sdfEdge          = 0.75f;
    private InputFontFormat inputFormat = InputFontFormat.FORMAT_TRUETYPE;
    private Stroke outlineStroke        = null;
    private int channelCount            = 3;
    private FontDesc fontDesc;
    private GlyphBank.Builder glyphBankBuilder;

    private ArrayList<Glyph> glyphs = new ArrayList<Glyph>();

    private Font font;
    private BMFont bmfont;

    public static long FontDescToHash(FontDesc fontDesc) {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();

        fontDescbuilder.mergeFrom(fontDesc);
        FontDesc desc = fontDescbuilder.build();

        // the list of parameters which affect the glyph_bank
        String result = ""
            + desc.getFont()
            + desc.getSize()
            + desc.getAntialias()
            + desc.getOutlineWidth()
            + desc.getShadowBlur()
            + desc.getCharacters()
            + desc.getOutputFormat()
            + desc.getAllChars()
            + desc.getCacheWidth()
            + desc.getCacheHeight()
            + desc.getRenderMode();

        return MurmurHash.hash64(result);
    }

    // These values are the same as font_renderer.cpp
    static final int LAYER_FACE    = 0x1;
    static final int LAYER_OUTLINE = 0x2;
    static final int LAYER_SHADOW  = 0x4;

    // The fontMapLayerMask contains a bitmask of the layers that should be rendered
    // by the font renderer. Note that this functionality requires that the render mode
    // property of the font resource must be set to MULTI_LAYER. Default behaviour
    // is to render the font as single layer (for compatability reasons).
    public static int GetFontMapLayerMask(FontDesc fontDesc)
    {
        int fontMapLayerMask = LAYER_FACE;

        if (fontDesc.getRenderMode() == FontRenderMode.MODE_MULTI_LAYER)
        {
            if (fontDesc.getOutlineAlpha() > 0 &&
                fontDesc.getOutlineWidth() > 0)
            {
                fontMapLayerMask |= LAYER_OUTLINE;
            }

            if (fontDesc.getShadowAlpha() > 0 &&
                fontDesc.getAlpha() > 0)
            {
                fontMapLayerMask |= LAYER_SHADOW;
            }
        }

        return fontMapLayerMask;
    }

    // used by the font builder
    public static int GetFontMapPadding(FontDesc fontDesc)
    {
        if (isBitmapFont(fontDesc)) {
            return 0;
        } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {
            // The +1 is needed to give a little bit of extra padding since the spread
            // always gets padded by the sqrt of a pixel diagonal
            return fontDesc.getShadowBlur() + (int)(fontDesc.getOutlineWidth()) + 1;
        } else {
            return Math.min(4, fontDesc.getShadowBlur()) + (int)(fontDesc.getOutlineWidth());
        }
    }

    public static float GetFontMapSdfSpread(FontDesc fontDesc)
    {
        return getPaddedSdfSpread(fontDesc.getOutlineWidth());
    }

    public static float GetFontMapSdfOutline(FontDesc fontDesc)
    {
        float sdfSpread = GetFontMapSdfSpread(fontDesc);
        return calculateSdfEdgeLimit(-fontDesc.getOutlineWidth(), sdfSpread);
    }

    public static float GetFontMapSdfShadow(FontDesc fontDesc)
    {
        float shadowBlur = (float)fontDesc.getShadowBlur();
        float sdfShadowSpread = getPaddedSdfSpread(shadowBlur);
        float shadowEdge = calculateSdfEdgeLimit(-shadowBlur, sdfShadowSpread);

        // Special case!
        // If there is no blur, the shadow should essentially work the same way as the outline.
        // This enables effects like a hard drop shadow. In the shader, the pseudo code
        // that does this looks something like this:
        // shadow_alpha = mix(shadow_alpha,outline_alpha,floor(shadowEdge))
        if (fontDesc.getShadowBlur() == 0)
        {
            shadowEdge = 1.0f;
        }
        return shadowEdge;
    }

    public interface FontResourceResolver {
        public InputStream getResource(String resourceName) throws FileNotFoundException;
    }

    public Fontc() {

    }

    public ArrayList<Glyph> getGlyphs() {
        return glyphs;
    }

    public GlyphBank getGlyphBank() {
        return glyphBankBuilder.build();
    }

    private static boolean isBitmapFont(FontDesc fd) {
        return StringUtil.toLowerCase(fd.getFont()).endsWith("fnt");
    }

    public void TTFBuilder(InputStream fontStream) throws FontFormatException, IOException {

        ArrayList<Integer> characters = new ArrayList<Integer>();

        if (!fontDesc.getAllChars()) {

            String chars = fontDesc.getCharacters();
            if (!StringUtils.isEmpty(chars)) {
                for (int i = 0; i < chars.length(); i++) {
                    char c = chars.charAt(i);
                    characters.add((int) c);
                }
            }
            else {
                for (char c : ASCII_7BIT) {
                    characters.add((int) c);
                }
                String extraCharacters = fontDesc.getExtraCharacters();
                for (int i = 0; i < extraCharacters.length(); i++) {
                    char c = extraCharacters.charAt(i);
                    characters.add((int) c);
                }
            }

            // remove duplicates AND sort (TreeSet is sorted)
            Set<Integer> deDup = new TreeSet<Integer>(characters);
            characters = new ArrayList<Integer>(deDup);
        }

        if (fontDesc.getOutlineWidth() > 0.0f) {
            outlineStroke = new BasicStroke(fontDesc.getOutlineWidth() * 2.0f);
        }

        font = Font.createFont(Font.TRUETYPE_FONT, fontStream);
        font = font.deriveFont(Font.PLAIN, fontDesc.getSize());


        FontRenderContext fontRendererContext = new FontRenderContext(new AffineTransform(), fontDesc.getAntialias() != 0, fontDesc.getAntialias() != 0);

        int loopEnd = characters.size();
        if (fontDesc.getAllChars()) {
            assert(0 == loopEnd);
            loopEnd = 0x10FFFF;
        }
        for (int i = 0; i < loopEnd; ++i) {

            int codePoint = i;
            if (!fontDesc.getAllChars()) {
                codePoint = characters.get(i);
            }

            if (font.canDisplay(codePoint)) {
                String s = new String(Character.toChars(codePoint));

                GlyphVector glyphVector = font.createGlyphVector(fontRendererContext, s);
                Rectangle visualBounds = glyphVector.getOutline().getBounds();

                GlyphMetrics metrics = glyphVector.getGlyphMetrics(0);

                Glyph glyph = new Glyph();
                glyph.ascent = (int)Math.ceil(-visualBounds.getMinY());
                glyph.descent = (int)Math.ceil(visualBounds.getMaxY());

                glyph.c = codePoint;
                glyph.index = i;
                glyph.advance = Math.round(metrics.getAdvance());
                float leftBearing = metrics.getLSB();
                glyph.leftBearing = (int)Math.floor(leftBearing);
                glyph.width = visualBounds.width;
                if (leftBearing != 0.0f) {
                    glyph.width += 1;
                }

                glyph.vector = glyphVector;
                if (!(glyph.width == 0 && glyph.advance == 0 && codePoint >= 65000)) {
                    glyphs.add(glyph);
                }
            }
        }

        BufferedImage image = new BufferedImage(256, 256, BufferedImage.TYPE_3BYTE_BGR);
        Graphics2D g        = image.createGraphics();
        g.setBackground(Color.BLACK);
        g.clearRect(0, 0, image.getWidth(), image.getHeight());
        setHighQuality(g);

        FontMetrics fontMetrics = g.getFontMetrics(font);

        Rectangle2D rect = fontMetrics.getMaxCharBounds(g);

        glyphBankBuilder.setMaxAscent(fontMetrics.getMaxAscent())
                        .setMaxDescent(fontMetrics.getMaxDescent())
                        .setMaxAdvance(fontMetrics.getMaxAdvance())
                        .setMaxWidth((float)rect.getWidth())
                        .setMaxHeight((float)rect.getHeight());
    }


    public void FNTBuilder(InputStream fontStream) throws FontFormatException, IOException {
        this.inputFormat = InputFontFormat.FORMAT_BMFONT;

        bmfont = new BMFont();

        // parse BMFont file
        try {
            bmfont.parse(fontStream);
        } catch (BMFontFormatException e) {
            throw new FontFormatException(e.getMessage());
        }

        int maxAscent = 0;
        int maxDescent = 0;
        for (int i = 0; i < bmfont.charArray.size(); ++i) {

            Char c = bmfont.charArray.get(i);

            int ascent = bmfont.base - (int)c.yoffset;
            int descent = c.height - ascent;

            maxAscent = Math.max(ascent, maxAscent);
            maxDescent = Math.max(descent, maxDescent);

            Glyph glyph = new Glyph();
            glyph.ascent = ascent;
            glyph.descent = descent;

            glyph.x = c.x;
            glyph.y = c.y;
            glyph.c = c.id;
            glyph.index = i;
            glyph.advance = (int) c.xadvance;
            glyph.leftBearing = (int) c.xoffset;
            glyph.width = c.width;

            glyphs.add(glyph);
        }

        glyphBankBuilder.setMaxAscent(maxAscent)
                      .setMaxDescent(maxDescent);
    }

    private static float getPaddedSdfSpread(float spreadInput)
    {
        // Make sure the output spread value is not zero. We distribute the distance values over
        // the spread when we generate the DF glyphs, so if this value is zero we won't be able to map
        // the distance values to a valid range..
        // We use sqrt(2) since it is the diagonal length of a pixel, but any small positive value would do.
        float  sqrt2 = 1.4142f;
        return sqrt2 + spreadInput;
    }

    private static float calculateSdfEdgeLimit(float width, float spread)
    {
        // Normalize the incoming value to [-1,1]
        float sdfLimitValue = width / spread;

        // Map the outgoing limit to the same 'space' as the face edge.
        return sdfLimitValue * (1.0f - sdfEdge) + sdfEdge;
    }

    private byte[] toByteArray(BufferedImage image, int width, int height, int bpp, int targetBpp) throws IOException {
        int dataSize = width * height * bpp;
        byte[] tmp = new byte[dataSize];
        int cursor = 0;

        int[] rasterData = new int[width * height * 4];
        image.getRaster().getPixels(0, 0, width, height, rasterData);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // BGRA to RGBA
                int i = y*width*bpp + x*bpp;
                int green = 0;
                int red = 0;
                int alpha = 0;
                int blue = rasterData[i + 0];
                if (bpp > 1)
                    green = rasterData[i + 1];
                if (bpp > 2)
                    red = rasterData[i + 2];
                if (bpp > 3)
                    alpha = rasterData[i + 3];

                tmp[cursor++] = (byte)(red & 0xFF);
                if (targetBpp > 1)
                    tmp[cursor++] = (byte)(green & 0xFF);
                if (targetBpp > 2)
                    tmp[cursor++] = (byte)(blue & 0xFF);
                if (targetBpp > 3)
                    tmp[cursor++] = (byte)(alpha & 0xFF);
            }
        }
        byte[] out = new byte[cursor];
        System.arraycopy(tmp, 0, out, 0, cursor);
        return out;
    }

    public BufferedImage generateGlyphData(boolean preview, final FontResourceResolver resourceResolver) throws TextureGeneratorException, FontFormatException {

        ByteArrayOutputStream glyphDataBank = new ByteArrayOutputStream(1024*1024*4);

        // Padding is the pixel amount needed to get a good antialiasing around the glyphs, while cell padding
        // is the extra padding added to the bitmap data to avoid filtering glitches when rendered.
        int padding = GetFontMapPadding(fontDesc);
        int cell_padding = 1;
        // Spread is the maximum distance to the glyph edge.
        float sdfSpread = 0.0f;
        // Shadow_spread is the maximum distance to the glyph outline.
        float sdfShadowSpread = 0.0f;

        if (Fontc.isBitmapFont(this.fontDesc)) {
            padding = 0;
            cell_padding = 1;
        } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {
            sdfSpread        = getPaddedSdfSpread(fontDesc.getOutlineWidth());
            sdfShadowSpread = getPaddedSdfSpread((float)fontDesc.getShadowBlur());
        }

        Color faceColor = new Color(fontDesc.getAlpha(), 0.0f, 0.0f);
        Color outlineColor = new Color(0.0f, fontDesc.getOutlineAlpha(), 0.0f);
        ConvolveOp shadowConvolve = null;
        Composite blendComposite = new BlendComposite();
        if (fontDesc.getShadowAlpha() > 0.0f) {
            float[] kernelData = {
                    0.0625f, 0.1250f, 0.0625f,
                    0.1250f, 0.2500f, 0.1250f,
                    0.0625f, 0.1250f, 0.0625f
            };
            Kernel kernel = new Kernel(3, 3, kernelData);
            RenderingHints hints = new RenderingHints(null);
            hints.put(RenderingHints.KEY_COLOR_RENDERING, RenderingHints.VALUE_COLOR_RENDER_QUALITY);
            hints.put(RenderingHints.KEY_DITHERING, RenderingHints.VALUE_DITHER_DISABLE);
            shadowConvolve = new ConvolveOp(kernel, ConvolveOp.EDGE_NO_OP, hints);
        }
        if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {
            glyphBankBuilder.setSdfSpread(GetFontMapSdfSpread(fontDesc));
            glyphBankBuilder.setSdfOutline(GetFontMapSdfOutline(fontDesc));
            glyphBankBuilder.setSdfShadow(GetFontMapSdfShadow(fontDesc));
        }

        // Load external image resource for BMFont files
        BufferedImage imageBMFont = null;
        if (inputFormat == InputFontFormat.FORMAT_BMFONT) {
            String origPath = Paths.get(FilenameUtils.normalize(bmfont.page.get(0))).getFileName().toString();
            InputStream inputImageStream = null;
            try {
                inputImageStream = resourceResolver.getResource(origPath);
                imageBMFont = ImageIO.read(inputImageStream);
                inputImageStream.close();
            } catch (FileNotFoundException e) {
                throw new FontFormatException("Could not find BMFont image resource: " + origPath);
            } catch (IOException e) {
                throw new FontFormatException("Error while reading BMFont image resource: " + e.getMessage());
            }
        }

        // Calculate channel count depending on both input and output format
        if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP &&
            inputFormat == InputFontFormat.FORMAT_TRUETYPE) {

            // If font has outline or shadow, we need all three channels
            if ((fontDesc.getOutlineWidth() > 0.0f && this.fontDesc.getOutlineAlpha() > 0.0f) || (fontDesc.getShadowAlpha() > 0.0f)) {
                channelCount = 3;
            } else {
                channelCount = 1;
            }

        } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP && inputFormat == InputFontFormat.FORMAT_BMFONT) {
            channelCount = 4;
        } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD && inputFormat == InputFontFormat.FORMAT_TRUETYPE) {
            // If font has shadow and blur, we'll need 3 channels. Not all platforms universally support
            // texture formats with only 2 channels (such as LUMINANCE_ALPHA)
            if (fontDesc.getShadowBlur() > 0.0f && fontDesc.getShadowAlpha() > 0.0f) {
                channelCount = 3;
            }
            else {
                channelCount = 1;
            }
        }

        // We keep track of offset into the glyph data bank,
        // this is saved for each glyph to know where their bitmap data is stored.
        int dataOffset = 0;

        // Find max width, height for each glyph to lock down cache cell sizes,
        // this includes cell padding.
        int cell_width = 0;
        int cell_height = 0;

        // Find max glyph ascent and descent for each glyph. This is needed
        // since we cannot fully trust or use the values from the getMaxAscent / getMaxDescent
        // These values are are used both for determining the cache cell size as well
        // as y-offset in the cache subimage update routines.
        int cell_max_ascent  = 0;
        int cell_max_descent = 0;

        for (int i = 0; i < glyphs.size(); i++) {
            Glyph glyph = glyphs.get(i);
            if (glyph.width <= 0.0f) {
                continue;
            }

            int ascent  = glyph.ascent;
            int descent = glyph.descent;
            int width   = glyph.width + padding * 2 + cell_padding * 2;

            cell_width       = Math.max(cell_width, width);
            cell_max_ascent  = Math.max(cell_max_ascent, ascent);
            cell_max_descent = Math.max(cell_max_descent, descent);
        }

        cell_height       = cell_max_ascent + cell_max_descent + padding * 2 + cell_padding * 2;
        cell_max_ascent  += padding;

        // Some hardware don't like doing subimage updates on non-aligned cell positions.
        if (channelCount == 3) {
            cell_width = (int)Math.ceil((double)cell_width / 4.0) * 4;
        }

        // We do an early cache size calculation before we create the glyph bitmaps
        // This is so that we can know when we have created enough glyphs to fill
        // the cache when creating a preview image.
        int cache_width = 1024;
        int cache_height = 0;
        if (fontDesc.getCacheWidth() > 0) {
            cache_width = fontDesc.getCacheWidth();
        }
        int cache_columns = cache_width / cell_width;

        if (fontDesc.getCacheHeight() > 0) {
            cache_height = fontDesc.getCacheHeight();
        } else {
            // No "static" height set, guess height size for all to fit
            int tot_rows = (int)Math.ceil((double)glyphs.size() / (double)cache_columns);
            int tot_height = tot_rows * cell_height;
            tot_height = (int) (Math.log(tot_height) / Math.log(2));
            tot_height = (int) Math.pow(2, tot_height + 1);
            cache_height = Math.min(tot_height,  2048);
        }
        int cache_rows = cache_height / cell_height;

        int include_glyph_count = glyphs.size();

        if (preview) {
            include_glyph_count = Math.min(glyphs.size(), cache_rows * cache_columns);
        }

        for (int i = 0; i < include_glyph_count; i++) {

            Glyph glyph = glyphs.get(i);
            if (glyph.width <= 0 || glyph.ascent + glyph.descent <= 0) {
                continue;
            }

            // Generate bitmap for each glyph depending on format
            BufferedImage glyphImage = null;
            if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP &&
                inputFormat == InputFontFormat.FORMAT_TRUETYPE) {
                glyphImage = drawGlyph(glyph, padding, font, blendComposite, faceColor, outlineColor, shadowConvolve);
            } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP &&
                       inputFormat == InputFontFormat.FORMAT_BMFONT) {
                glyphImage = drawBMFontGlyph(glyph, imageBMFont);
            } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD &&
                       inputFormat == InputFontFormat.FORMAT_TRUETYPE) {
                glyphImage = makeDistanceField(glyph, padding, sdfSpread, sdfShadowSpread, font, sdfEdge, shadowConvolve);
            } else {
                throw new FontFormatException("Invalid font format combination!");
            }

            if (preview) {

                glyph.image = glyphImage;

            } else {
                BufferedImage paddedGlyphImage = new BufferedImage(glyphImage.getWidth() + cell_padding * 2,
                                                                    glyphImage.getHeight() + cell_padding * 2, BufferedImage.TYPE_4BYTE_ABGR);

                int clearData = 0;
                int mask = 0xFFFFFFFF;
                if (channelCount==1)
                    mask = 0xFF;
                else if (channelCount==2)
                    mask = 0xFFFF;
                else if (channelCount==3)
                    mask = 0xFFFFFF;

                int py = 0;
                // Get raster data from rendered glyph and store in glyph data bank
                for (int x = 0; x < paddedGlyphImage.getWidth(); ++x) {
                    paddedGlyphImage.setRGB(x, py, clearData);
                }
                py++;
                for (int y = 0; y < glyphImage.getHeight(); y++, py++) {
                    int px = 0;
                    paddedGlyphImage.setRGB(px++, py, clearData);
                    for (int x = 0; x < glyphImage.getWidth(); x++, px++) {
                        int color = glyphImage.getRGB(x, y);
                        int blue  = (color) & 0xff;
                        int green = (color >> 8) & 0xff;
                        int red   = (color >> 16) & 0xff;
                        int alpha = (color >> 24) & 0xff;
                        blue = (blue * alpha) / 255;
                        green = (green * alpha) / 255;
                        red = (red * alpha) / 255;
                        color = ((alpha << 24) |
                                (blue << 16) |
                                (green << 8) |
                                (red << 0)) & mask;

                        paddedGlyphImage.setRGB(px, py, color);
                    }
                    paddedGlyphImage.setRGB(px++, py, clearData);
                }
                for (int x = 0; x < paddedGlyphImage.getWidth(); ++x) {
                    paddedGlyphImage.setRGB(x, py, clearData);
                }

                try {
                    int width = paddedGlyphImage.getWidth();
                    int height = paddedGlyphImage.getHeight();

                    byte[] uncompressedBytes = toByteArray(paddedGlyphImage, width, height, 4, channelCount);

                    Texc.Buffer compressedBuffer = TexcLibraryJni.CompressBuffer(uncompressedBytes);
                    byte[] compressedBytes = compressedBuffer.data;

                    // If the uncompressed size is smaller we write uncompressed bytes instead
                    // Note that when writing the uncompressed bytes we need to
                    // also write the initial byte/flag telling the consumer if
                    // the glyph is compressed or not.
                    boolean useCompressed = compressedBuffer.isCompressed && compressedBytes.length < uncompressedBytes.length;
                    byte[] bytes = useCompressed ? compressedBytes : uncompressedBytes;

                    glyphDataBank.write(useCompressed ? 1 : 0); // the "header"
                    glyphDataBank.write(bytes);

                    glyph.cache_entry_offset = dataOffset;
                    glyph.cache_entry_size = 1 + bytes.length;
                    dataOffset += glyph.cache_entry_size;
                } catch(IOException e) {
                    throw new TextureGeneratorException(String.format("Failed to generate font texture: %s", e.getMessage()));
                }
            }
        }

        // Sanity check;
        // Some fonts don't include ASCII range and trying to compile a font without
        // setting "all_chars" could result in an empty glyph list.
        if (glyphs.size() == 0) {
            throw new FontFormatException("No character glyphs where included! Maybe turn on 'all_chars'?");
        }

        // Start filling the rest of FontMap
        glyphBankBuilder.setGlyphPadding(cell_padding);
        glyphBankBuilder.setCacheWidth(cache_width);
        glyphBankBuilder.setCacheHeight(cache_height);
        glyphBankBuilder.setGlyphData(ByteString.copyFrom(glyphDataBank.toByteArray()));
        glyphBankBuilder.setCacheCellWidth(cell_width);
        glyphBankBuilder.setCacheCellHeight(cell_height);
        glyphBankBuilder.setGlyphChannels(channelCount);
        glyphBankBuilder.setCacheCellMaxAscent(cell_max_ascent);

        BufferedImage previewImage = null;
        if (preview) {
            try {
                previewImage = generatePreviewImage();
            } catch (IOException e) {
                throw new FontFormatException("Could not generate font preview: " + e.getMessage());
            }
        }

        boolean monospace = true;
        int prevAdvance = -1;

        for (int i = 0; i < include_glyph_count; i++) {
            Glyph glyph = glyphs.get(i);
            // The Defold generator clips the SDF image but the stb generator does not.
            // So, we need to have a width, and an image width, in order to handle the different values at runtime
            int width = glyph.width + (glyph.width > 0 ? padding * 2 : 0);
            GlyphBank.Glyph.Builder glyphBuilder = GlyphBank.Glyph.newBuilder()
                .setCharacter(glyph.c)
                .setWidth(width)
                .setAdvance(glyph.advance)
                .setLeftBearing(glyph.leftBearing)
                .setAscent(glyph.ascent + padding)
                .setDescent(glyph.descent + padding)
                .setGlyphDataOffset(glyph.cache_entry_offset)
                .setGlyphDataSize(glyph.cache_entry_size);

            // "Static" cache positions is only used for previews currently
            if (preview) {
                glyphBuilder.setX(glyph.x);
                glyphBuilder.setY(glyph.y);
            }

            glyphBankBuilder.addGlyphs(glyphBuilder);

            if (prevAdvance == -1)
                prevAdvance = glyph.advance;

            monospace = monospace && (prevAdvance == glyph.advance);
            prevAdvance = glyph.advance;
        }

        if (include_glyph_count <= 1)
            monospace = false; // it's likely a dynamic font

        glyphBankBuilder.setIsMonospaced(monospace);
        glyphBankBuilder.setPadding(padding);
        return previewImage;

    }

    private BufferedImage drawBMFontGlyph(Glyph glyph, BufferedImage imageBMFontInput) {
        return imageBMFontInput.getSubimage(glyph.x, glyph.y, glyph.width, glyph.ascent + glyph.descent);
    }

    private BufferedImage makeDistanceField(Glyph glyph, int padding, float sdfSpread, float sdfShadowSpread, Font font, float edge, ConvolveOp shadowConvolve) {
        int width = glyph.width + padding * 2;
        int height = glyph.ascent + glyph.descent + padding * 2;

        Shape sh = glyph.vector.getGlyphOutline(0);
        // Normalize the outline by boolean-unioning overlapping contours to avoid
        // internal edges contributing to the distance field (fixes artifacts for
        // glyphs composed of multiple vector shapes; see issue #6577)
        Area area = new Area(sh);
        PathIterator pi = area.getPathIterator(new AffineTransform(1,0,0,1,0,0));
        pi = new FlatteningPathIterator(pi,  0.1);

        double _x = 0, _y = 0;
        double _lastmx = 0, _lastmy = 0;
        DistanceFieldGenerator df = new DistanceFieldGenerator();
        while (!pi.isDone()) {
            double [] c = new double[100];
            int res = pi.currentSegment(c);
            switch (res) {
              case PathIterator.SEG_MOVETO:
                  _x = c[0];
                  _y = c[1];
                  _lastmx = _x;
                  _lastmy = _y;
                  break;
              case PathIterator.SEG_LINETO:
                  df.addLine(_x, _y, c[0], c[1]);
                  _x = c[0];
                  _y = c[1];
                  break;
              case PathIterator.SEG_CLOSE:
                  df.addLine(_x, _y, _lastmx, _lastmy);
                  _x = _lastmx;
                  _y = _lastmy;
                  break;
              default:
                  break;
            }
            pi.next();
        }

        glyph.x = -glyph.leftBearing + padding;
        glyph.y =  glyph.ascent + padding;

        // Glyph vector coordinates of the destination rect.
        double u0 = -glyph.x;
        double v0 = -glyph.y;
        double u1 = u0 + width;
        double v1 = v0 + height;

        double[] distanceData = new double[width*height];

        df.render(distanceData, u0, v0, u1, v1, width, height);

        double widthInverse  = 1 / (double)width;
        double heightInverse = 1 / (double)height;

        BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_3BYTE_BGR);

        // TODO: Split this work into a pre-pass and subsequent face/outline & shadow passes
        for (int v=0;v<height;v++) {
            int ofs = v * width;
            for (int u=0;u<width;u++) {
                double gx = u0 + widthInverse * u * (u1 - u0);
                double gy = v0 + heightInverse * v * (v1 - v0);
                double distanceToEdge   = distanceData[ofs + u];
                double distanceToBorder = -(distanceToEdge - fontDesc.getOutlineWidth());

                if (!area.contains(gx, gy)) {
                    distanceToEdge = -distanceToEdge;
                }

                float distanceToEdgeNormalized = calculateSdfEdgeLimit((float)distanceToEdge, sdfSpread);

                // Map outgoing distance value to 0..255
                int outlineChannel = (int)(255.0f * distanceToEdgeNormalized);
                outlineChannel     = Math.max(0,Math.min(255,outlineChannel));

                float sdfOutline = glyphBankBuilder.getSdfOutline();

                // This is needed to 'fill' the shadow body since
                // we have no good way of knowing if the pixel is inside or outside
                // of the shadow limit
                if (distanceToEdgeNormalized > sdfOutline)
                {
                	distanceToBorder = edge;
                }

                // Calculate shadow distribution separately in a different channel since we spread the
                // values across the distance to the outline border and not to the edge.
                float distanceToBorder_normalized = calculateSdfEdgeLimit((float)distanceToBorder, sdfShadowSpread);

                // Map outgoing distance value to 0..255
                int shadowChannel = (int)(255.0f * distanceToBorder_normalized);
                shadowChannel     = Math.max(0,Math.min(255,shadowChannel));

                image.setRGB(u, v, 0x010000 * outlineChannel | 0x000001 * shadowChannel);
            }
        }

        if (fontDesc.getShadowAlpha() > 0.0f && fontDesc.getShadowBlur() > 0) {

            BufferedImage blurredShadowImage = new BufferedImage(width,height, BufferedImage.TYPE_3BYTE_BGR);
            Graphics2D g = blurredShadowImage.createGraphics();
            setHighQuality(g);
            g.drawImage(image, 0, 0, null);

            // When the blur kernel is != 0, make sure to always blur the DF data set
            // at least once so we can avoid the jaggies around the face edges. This is mostly
            // prominent when the blur size is small and the offset is large.
            BufferedImage tmp = blurredShadowImage.getSubimage(0, 0, width, height);
            shadowConvolve.filter(tmp, blurredShadowImage);

            for (int v=0;v<height;v++) {
                for (int u=0;u<width;u++) {
                    int edgeOutlineChannel = image.getRGB(u,v);
                    int shadowChannel = blurredShadowImage.getRGB(u,v);

                    image.setRGB(u,v,edgeOutlineChannel & 0xFFFF00 | shadowChannel & 0xFF);
                }
            }
        }

        return image;
    }

    private BufferedImage drawGlyph(Glyph glyph, int padding, Font font, Composite blendComposite, Color faceColor, Color outlineColor, ConvolveOp shadowConvolve) {
        int width = glyph.width + padding * 2;
        int height = glyph.ascent + glyph.descent + padding * 2;

        int dx = -glyph.leftBearing + padding;
        int dy = glyph.ascent + padding;

        glyph.x = dx;
        glyph.y = dy;

        BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_3BYTE_BGR);
        Graphics2D g = image.createGraphics();
        setHighQuality(g);
        g.setBackground(Color.BLACK);
        g.clearRect(0, 0, image.getWidth(), image.getHeight());
        g.translate(dx, dy);

        Shape outline = glyph.vector.getOutline(0, 0);
        if (this.fontDesc.getShadowAlpha() > 0.0f) {
            if (this.fontDesc.getAlpha() > 0.0f) {
                g.setPaint(new Color(0.0f, 0.0f, this.fontDesc.getShadowAlpha() * this.fontDesc.getAlpha()));
                g.fill(outline);
            }
            if (this.outlineStroke != null && this.fontDesc.getOutlineAlpha() > 0.0f) {
                g.setPaint(new Color(0.0f, 0.0f, this.fontDesc.getShadowAlpha() * this.fontDesc.getOutlineAlpha()));
                g.setStroke(this.outlineStroke);
                g.draw(outline);
            }
            for (int pass = 0; pass < this.fontDesc.getShadowBlur(); ++pass) {
                BufferedImage tmp = image.getSubimage(0, 0, width, height);
                shadowConvolve.filter(tmp, image);
            }
        }

        g.setComposite(blendComposite);
        if (this.outlineStroke != null && this.fontDesc.getOutlineAlpha() > 0.0f) {
            g.setPaint(outlineColor);
            g.setStroke(this.outlineStroke);
            g.draw(outline);
        }

        if (this.fontDesc.getAlpha() > 0.0f) {
            g.setPaint(faceColor);
            g.fill(outline);
        }

        return image;
    }

    private void setHighQuality(Graphics2D g) {
        g.setRenderingHint(RenderingHints.KEY_ALPHA_INTERPOLATION,
            RenderingHints.VALUE_ALPHA_INTERPOLATION_QUALITY);
        g.setRenderingHint(RenderingHints.KEY_COLOR_RENDERING,
            RenderingHints.VALUE_COLOR_RENDER_QUALITY);
        g.setRenderingHint(RenderingHints.KEY_DITHERING,
            RenderingHints.VALUE_DITHER_DISABLE);
        g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,
            RenderingHints.VALUE_INTERPOLATION_BICUBIC);
        g.setRenderingHint(RenderingHints.KEY_RENDERING,
            RenderingHints.VALUE_RENDER_QUALITY);
        if (this.fontDesc.getAntialias() != 0) {
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                RenderingHints.VALUE_ANTIALIAS_ON);
            g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL,
                RenderingHints.VALUE_STROKE_PURE);
            g.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS,
                RenderingHints.VALUE_FRACTIONALMETRICS_ON);
            g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING,
                RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
       } else {
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                RenderingHints.VALUE_ANTIALIAS_OFF);
            g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL,
                RenderingHints.VALUE_STROKE_NORMALIZE);
            g.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS,
                RenderingHints.VALUE_FRACTIONALMETRICS_OFF);
            g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING,
                RenderingHints.VALUE_TEXT_ANTIALIAS_OFF);
       }
    }

    public BufferedImage compile(InputStream fontStream, FontDesc fontDesc, boolean preview, final FontResourceResolver resourceResolver) throws FontFormatException, TextureGeneratorException, IOException {
        this.fontDesc       = fontDesc;
        this.glyphBankBuilder = GlyphBank.newBuilder();
        this.glyphBankBuilder.setImageFormat(fontDesc.getOutputFormat());

        if (Fontc.isBitmapFont(fontDesc)) {
            FNTBuilder(fontStream);
        } else {
            TTFBuilder(fontStream);
        }

        return generateGlyphData(preview, resourceResolver);
    }

    public BufferedImage generatePreviewImage() throws IOException {

        Graphics2D g;
        BufferedImage previewImage = new BufferedImage(glyphBankBuilder.getCacheWidth(), glyphBankBuilder.getCacheHeight(), BufferedImage.TYPE_3BYTE_BGR);
        g = previewImage.createGraphics();
        g.setBackground(Color.BLACK);
        g.clearRect(0, 0, previewImage.getWidth(), previewImage.getHeight());
        setHighQuality(g);

        int cache_columns = glyphBankBuilder.getCacheWidth() / glyphBankBuilder.getCacheCellWidth();
        int cache_rows = glyphBankBuilder.getCacheHeight() / glyphBankBuilder.getCacheCellHeight();

        BufferedImage glyphImage;
        for (int i = 0; i < glyphs.size(); i++) {

            Glyph glyph = glyphs.get(i);

            int col = i % cache_columns;
            int row = i / cache_columns;

            if (row >= cache_rows) {
                break;
            }

            int x = col * glyphBankBuilder.getCacheCellWidth();
            int y = row * glyphBankBuilder.getCacheCellHeight();

            glyph.x = x;
            glyph.y = y;

            g.translate(x, y);
            glyphImage = glyph.image;
            g.drawImage(glyphImage, 0, 0, null);
            g.translate(-x, -y);

        }

        return previewImage;
    }

    private static void Usage() {
        System.err.println("Usage: fontc fontfile outfile [basedir] [dynamic]");
        System.exit(1);
    }

    // run with java -cp bob.jar com.dynamo.bob.font.Fontc foobar.font foobar.fontc
    public static void main(String[] args) throws FontFormatException, TextureGeneratorException {
        try {
            System.setProperty("java.awt.headless", "true");

            File    fontInput   = null;
            String  outfile     = null;
            String  basedir     = ".";
            boolean dynamic     = false;

            if (args.length >= 1) {
                fontInput = new File(args[0]);
            }

            if (args.length >= 2) {
                outfile = args[1];
            }

            if (args.length >= 3) {
                basedir = args[2];
            }

            if (args.length >= 4) {
                dynamic = Boolean.parseBoolean(args[3]);
            }

            if (fontInput == null)
            {
                System.err.println("No input .font specified!");
                Usage();
            }

            if (outfile == null)
            {
                System.err.println("No output file specified!");
                Usage();
            }

            FileInputStream stream = new FileInputStream(fontInput);
            InputStreamReader reader = new InputStreamReader(stream);
            FontDesc.Builder builder = FontDesc.newBuilder();
            TextFormat.merge(reader, builder);
            reader.close();
            FontDesc fontDesc = builder.build();
            if (fontDesc.getFont().length() == 0) {
                System.err.println("No ttf font specified in " + args[0] + ".");
                System.exit(1);
            }
            String ttfFileName = basedir + File.separator + fontDesc.getFont();
            File ttfFile = new File(ttfFileName);
            if (!ttfFile.exists()) {
                System.err.println(String.format("%s:0 error: is missing the dependent ttf-file '%s'", args[0], fontDesc.getFont()));
                System.exit(1);
            }

            String materialFileName = basedir + File.separator + fontDesc.getMaterial();
            File materialFile = new File(materialFileName);
            if (!materialFile.isFile()) {
                System.err.println(String.format("%s:0 error: is missing the dependent material-file '%s'", args[0], fontDesc.getMaterial()));
                System.exit(1);
            }

            // Compile fontdesc to fontmap
            final String parentDir = fontInput.getParent();
            Fontc fontc = new Fontc();
            String fontInputFile = basedir + File.separator + fontDesc.getFont();
            BufferedInputStream fontInputStream = new BufferedInputStream(new FileInputStream(fontInputFile));
            BufferedImage previewImage = fontc.compile(fontInputStream, fontDesc, false, resourceName -> {
                Path resPath = Paths.get(parentDir, resourceName);
                BufferedInputStream resStream = new BufferedInputStream(new FileInputStream(resPath.toString()));

                return resStream;
            });
            fontInputStream.close();

            if (previewImage != null) {
                ImageIO.write(previewImage, "png", new File(outfile + "_preview.png"));
            }

            Path basedirAbsolutePath = Paths.get(basedir).toAbsolutePath();

            FontMap.Builder fontMapBuilder = FontMap.newBuilder();
            fontMapBuilder.setMaterial(BuilderUtil.replaceExt(fontDesc.getMaterial(), ".material", ".materialc"));

            if (!dynamic)
            {
                // Construct the project-relative path based from the input font file
                Path glyphBankProjectPath  = Paths.get(fontInput.getAbsolutePath().replace(".font", ".glyph_bankc"));
                Path glyphBankRelativePath = basedirAbsolutePath.relativize(glyphBankProjectPath);
                String glyphBankProjectStr = "/" + glyphBankRelativePath.toString().replace("\\","/");

                fontMapBuilder.setGlyphBank(glyphBankProjectStr);

                // Write glyph bank next to the output .fontc file
                String glyphBankOutputPath             = outfile.replace(".fontc", ".glyph_bankc");
                FileOutputStream glyphBankOutputStream = new FileOutputStream(glyphBankOutputPath);
                fontc.getGlyphBank().writeTo(glyphBankOutputStream);
            }
            else
            {
                if (fontDesc.getOutputFormat() != FontTextureFormat.TYPE_DISTANCE_FIELD) {
                    System.err.printf("Dynamic fonts currently only support distance fields! '%s'\n", fontInput);
                    System.exit(1);
                }

                fontMapBuilder.setFont(fontDesc.getFont());

                File src = new File(fontInputFile);

                Path fontProjectPath  = Paths.get(fontInput.getAbsolutePath());
                Path ttfRelativePath = basedirAbsolutePath.relativize(fontProjectPath.getParent());

                File outFontc = new File(outfile);
                File dst = new File(outFontc.getParent(), src.getName());
                Files.copy(src.toPath(), dst.toPath(), StandardCopyOption.REPLACE_EXISTING);
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
            fontMapBuilder.setLayerMask(GetFontMapLayerMask(fontDesc));

            fontMapBuilder.setOutputFormat(fontDesc.getOutputFormat());
            fontMapBuilder.setRenderMode(fontDesc.getRenderMode());

            // Write fontmap file
            FileOutputStream fontMapOutputStream = new FileOutputStream(outfile);

            fontMapBuilder.build().writeTo(fontMapOutputStream);

            fontMapOutputStream.close();

        } catch (IOException e) {
            System.err.println(e.getMessage());
            System.exit(1);
        }
    }
}
