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
import java.util.ArrayList;
import java.util.Comparator;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.font.BMFont.BMFontFormatException;
import com.dynamo.bob.font.BMFont.Char;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontTextureFormat;
import com.dynamo.render.proto.Font.FontRenderMode;
import com.google.protobuf.ByteString;
import com.google.protobuf.TextFormat;

class Glyph {
    int index;
    int c;
    int width;
    int advance;
    int leftBearing;
    int ascent;
    int descent;
    int x;
    int y;
    int cache_entry_offset;
    int cache_entry_size;
    GlyphVector vector;
    BufferedImage image;
};

class OrderComparator implements Comparator<Glyph> {
    @Override
    public int compare(Glyph o1, Glyph o2) {
        return (new Integer(o1.index)).compareTo(o2.index);
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

    public enum InputFontFormat {
        FORMAT_TRUETYPE, FORMAT_BMFONT
    };

    // These values are the same as font_renderer.cpp
    static final int LAYER_FACE    = 0x1;
    static final int LAYER_OUTLINE = 0x2;
    static final int LAYER_SHADOW  = 0x4;

    static final float sdf_edge = 0.75f;

    private InputFontFormat inputFormat = InputFontFormat.FORMAT_TRUETYPE;
    private Stroke outlineStroke = null;
    private int channelCount = 3;
    private FontDesc fontDesc;
    private FontMap.Builder fontMapBuilder;
    private ArrayList<Glyph> glyphs = new ArrayList<Glyph>();

    private Font font;
    private BMFont bmfont;

    public interface FontResourceResolver {
        public InputStream getResource(String resourceName) throws FileNotFoundException;
    }

    public Fontc() {

    }

    public InputFontFormat getInputFormat() {
        return inputFormat;
    }

    public void TTFBuilder(InputStream fontStream) throws FontFormatException, IOException {

        ArrayList<Integer> characters = new ArrayList<Integer>();

        if (!fontDesc.getAllChars()) {

            // 7-bit ASCII. Note inclusive range [32,126]
            for (int i = 32; i <= 126; ++i) {
                characters.add(i);
            }

            String extraCharacters = fontDesc.getExtraCharacters();
            for (int i = 0; i < extraCharacters.length(); i++) {
                char c = extraCharacters.charAt(i);
                characters.add((int)c);
            }

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

                glyphs.add(glyph);
            }
        }

        BufferedImage image;
        Graphics2D g;
        image = new BufferedImage(1024, 1024, BufferedImage.TYPE_3BYTE_BGR);
        g = image.createGraphics();
        g.setBackground(Color.BLACK);
        g.clearRect(0, 0, image.getWidth(), image.getHeight());
        setHighQuality(g);

        FontMetrics fontMetrics = g.getFontMetrics(font);
        int maxAscent = fontMetrics.getMaxAscent();
        int maxDescent = fontMetrics.getMaxDescent();
        fontMapBuilder.setMaxAscent(maxAscent)
                      .setMaxDescent(maxDescent)
                      .setShadowX(fontDesc.getShadowX())
                      .setShadowY(fontDesc.getShadowY());

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

        fontMapBuilder.setMaxAscent(maxAscent)
                      .setMaxDescent(maxDescent);
    }

    // We use repeatedWrite to create a (cell) padding around the glyph bitmap data
    private void repeatedWrite(ByteArrayOutputStream dataOut, int repeat, int value) {
        for (int i = 0; i < repeat; i++) {
            dataOut.write(value);
        }
    }

    private float getPaddedSdfSpread(float spreadInput)
    {
        // Make sure the output spread value is not zero. We distribute the distance values over
        // the spread when we generate the DF glyphs, so if this value is zero we won't be able to map
        // the distance values to a valid range..
        // We use sqrt(2) since it is the diagonal length of a pixel, but any small positive value would do.
        float  sqrt2 = 1.4142f;
        return sqrt2 + spreadInput;
    }

    private float calculateSdfEdgeLimit(float width, float spread)
    {
        // Normalize the incoming value to [-1,1]
        float sdfLimitValue = width / spread;

        // Map the outgoing limit to the same 'space' as the face edge.
        return sdfLimitValue * (1.0f - sdf_edge) + sdf_edge;
    }

    public BufferedImage generateGlyphData(boolean preview, final FontResourceResolver resourceResolver) throws FontFormatException {

        ByteArrayOutputStream glyphDataBank = new ByteArrayOutputStream(1024*1024*4);

        // Padding is the pixel amount needed to get a good antialiasing around the glyphs, while cell padding
        // is the extra padding added to the bitmap data to avoid filtering glitches when rendered.
        int padding = 0;
        int cell_padding = 1;
        // Spread is the maximum distance to the glyph edge.
        float sdf_spread = 0.0f;
        // Shadow_spread is the maximum distance to the glyph outline.
        float sdf_shadow_spread = 0.0f;

        if (fontDesc.getAntialias() != 0) {
            if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {
                sdf_spread        = getPaddedSdfSpread(fontDesc.getOutlineWidth());
                sdf_shadow_spread = getPaddedSdfSpread((float)fontDesc.getShadowBlur());

                // The +1 is needed to give a little bit of extra padding since the spread
                // always gets padded by the sqrt of a pixel diagonal
                padding = fontDesc.getShadowBlur() + (int)(fontDesc.getOutlineWidth()) + 1;
            }
            else {
                padding = Math.min(4, fontDesc.getShadowBlur()) + (int)(fontDesc.getOutlineWidth());
            }
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
            // Calculate edge values for both outline and shadow. We must treat them differently
            // so that we don't use the same precision range for both edges
            float outline_edge = calculateSdfEdgeLimit(-fontDesc.getOutlineWidth(), sdf_spread);
            float shadow_edge  = calculateSdfEdgeLimit(-(float)fontDesc.getShadowBlur(), sdf_shadow_spread);

            // Special case!
            // If there is no blur, the shadow should essentially work the same way as the outline.
            // This enables effects like a hard drop shadow. In the shader, the pseudo code
            // that does this looks something like this:
            // shadow_alpha = mix(shadow_alpha,outline_alpha,floor(shadow_edge))
            if (fontDesc.getShadowBlur() == 0)
            {
                shadow_edge = 1.0f;
            }

            fontMapBuilder.setSdfSpread(sdf_spread);
            fontMapBuilder.setSdfOutline(outline_edge);
            fontMapBuilder.setSdfShadow(shadow_edge);
            fontMapBuilder.setAlpha(this.fontDesc.getAlpha());
            fontMapBuilder.setOutlineAlpha(this.fontDesc.getOutlineAlpha());
            fontMapBuilder.setShadowAlpha(this.fontDesc.getShadowAlpha());
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

            // If font has outline, we need all three channels
            if (fontDesc.getOutlineWidth() > 0.0f && this.fontDesc.getOutlineAlpha() > 0.0f) {
                channelCount = 3;
            } else {
                channelCount = 1;
            }

        } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP &&
                   inputFormat == InputFontFormat.FORMAT_BMFONT) {
            channelCount = 4;
        } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD &&
                   inputFormat == InputFontFormat.FORMAT_TRUETYPE) {
            // If font has shadow, we'll need 3 channels. Not all platforms universally support
            // texture formats with only 2 channels (such as LUMINANCE_ALPHA)
            if (fontDesc.getShadowBlur() > 0.0f && fontDesc.getShadowAlpha() > 0.0f) {
                channelCount = 3;
            }
            else {
                channelCount = 1;
            }
        }

        // The fontMapLayerMask contains a bitmask of the layers that should be rendered
        // by the font renderer. Note that this functionality requires that the render mode
        // property of the font resource must be set to MULTI_LAYER. Default behaviour
        // is to render the font as single layer (for compatability reasons).
        int fontMapLayerMask = getFontMapLayerMask();

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
            int clearData = 0;
            if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP &&
                inputFormat == InputFontFormat.FORMAT_TRUETYPE) {
                glyphImage = drawGlyph(glyph, padding, font, blendComposite, faceColor, outlineColor, shadowConvolve);
            } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP &&
                       inputFormat == InputFontFormat.FORMAT_BMFONT) {
                glyphImage = drawBMFontGlyph(glyph, imageBMFont);
            } else if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD &&
                       inputFormat == InputFontFormat.FORMAT_TRUETYPE) {
                glyphImage = makeDistanceField(glyph, padding, sdf_spread, sdf_shadow_spread, font, sdf_edge, shadowConvolve);
                clearData = 0;
            } else {
                throw new FontFormatException("Invalid font format combination!");
            }

            if (preview) {

                glyph.image = glyphImage;

            } else {

                glyph.cache_entry_offset = dataOffset;
                // Get raster data from rendered glyph and store in glyph data bank
                int dataSizeOut = (glyphImage.getWidth() + cell_padding * 2) * (glyphImage.getHeight() + cell_padding * 2) * channelCount;

                repeatedWrite(glyphDataBank, (glyphImage.getWidth() + cell_padding * 2) * channelCount, clearData);
                for (int y = 0; y < glyphImage.getHeight(); y++) {

                    repeatedWrite(glyphDataBank, channelCount, clearData);
                    for (int x = 0; x < glyphImage.getWidth(); x++) {

                        int color = glyphImage.getRGB(x, y);
                        int blue  = (color) & 0xff;
                        int green = (color >> 8) & 0xff;
                        int red   = (color >> 16) & 0xff;
                        int alpha = (color >> 24) & 0xff;
                        blue = (blue * alpha) / 255;
                        green = (green * alpha) / 255;
                        red = (red * alpha) / 255;


                        glyphDataBank.write((byte)red);

                        if (channelCount > 1) {
                            glyphDataBank.write((byte)green);
                            glyphDataBank.write((byte)blue);

                            if (channelCount > 3) {

                                glyphDataBank.write((byte)alpha);
                            }
                        }
                    }

                    repeatedWrite(glyphDataBank, channelCount, clearData);
                }
                repeatedWrite(glyphDataBank, (glyphImage.getWidth() + cell_padding * 2) * channelCount, clearData);
                dataOffset += dataSizeOut;
                glyph.cache_entry_size = dataOffset - glyph.cache_entry_offset;
            }

        }

        // Sanity check;
        // Some fonts don't include ASCII range and trying to compile a font without
        // setting "all_chars" could result in an empty glyph list.
        if (glyphs.size() == 0) {
            throw new FontFormatException("No character glyphs where included! Maybe turn on 'all_chars'?");
        }

        // Start filling the rest of FontMap
        fontMapBuilder.setGlyphPadding(cell_padding);
        fontMapBuilder.setCacheWidth(cache_width);
        fontMapBuilder.setCacheHeight(cache_height);
        fontMapBuilder.setGlyphData(ByteString.copyFrom(glyphDataBank.toByteArray()));
        fontMapBuilder.setCacheCellWidth(cell_width);
        fontMapBuilder.setCacheCellHeight(cell_height);
        fontMapBuilder.setGlyphChannels(channelCount);
        fontMapBuilder.setCacheCellMaxAscent(cell_max_ascent);
        fontMapBuilder.setLayerMask(fontMapLayerMask);

        BufferedImage previewImage = null;
        if (preview) {
            try {
                previewImage = generatePreviewImage();
            } catch (IOException e) {
                throw new FontFormatException("Could not generate font preview: " + e.getMessage());
            }
        }

        for (int i = 0; i < include_glyph_count; i++) {
            Glyph glyph = glyphs.get(i);
            FontMap.Glyph.Builder glyphBuilder = FontMap.Glyph.newBuilder()
                .setCharacter(glyph.c)
                .setWidth(glyph.width + (glyph.width > 0 ? padding * 2 : 0))
                .setAdvance(glyph.advance)
                .setLeftBearing(glyph.leftBearing - padding)
                .setAscent(glyph.ascent + padding)
                .setDescent(glyph.descent + padding)
                .setGlyphDataOffset(glyph.cache_entry_offset)
                .setGlyphDataSize(glyph.cache_entry_size);

            // "Static" cache positions is only used for previews currently
            if (preview) {
                glyphBuilder.setX(glyph.x);
                glyphBuilder.setY(glyph.y);
            }

            fontMapBuilder.addGlyphs(glyphBuilder);
        }

        return previewImage;

    }

    private BufferedImage drawBMFontGlyph(Glyph glyph, BufferedImage imageBMFontInput) {
        return imageBMFontInput.getSubimage(glyph.x, glyph.y, glyph.width, glyph.ascent + glyph.descent);
    }

    private BufferedImage makeDistanceField(Glyph glyph, int padding, float sdf_spread, float sdf_shadow_spread, Font font, float edge, ConvolveOp shadowConvolve) {
        int width = glyph.width + padding * 2;
        int height = glyph.ascent + glyph.descent + padding * 2;

        Shape sh = glyph.vector.getGlyphOutline(0);
        PathIterator pi = sh.getPathIterator(new AffineTransform(1,0,0,1,0,0));
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

        double[] distance_data = new double[width*height];

        df.render(distance_data, u0, v0, u1, v1, width, height);

        double widthInverse  = 1 / (double)width;
        double heightInverse = 1 / (double)height;

        BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_3BYTE_BGR);

        // TODO: Split this work into a pre-pass and subsequent face/outline & shadow passes
        for (int v=0;v<height;v++) {
            int ofs = v * width;
            for (int u=0;u<width;u++) {
                double gx = u0 + widthInverse * u * (u1 - u0);
                double gy = v0 + heightInverse * v * (v1 - v0);
                double distance_to_edge   = distance_data[ofs + u];
                double distance_to_border = -(distance_to_edge - fontDesc.getOutlineWidth());

                if (!sh.contains(gx, gy)) {
                    distance_to_edge = -distance_to_edge;
                }

                float distance_to_edge_normalized = calculateSdfEdgeLimit((float)distance_to_edge, sdf_spread);

                // Map outgoing distance value to 0..255
                int outline_channel = (int)(255.0f * distance_to_edge_normalized);
                outline_channel     = Math.max(0,Math.min(255,outline_channel));

                float sdf_outline = fontMapBuilder.getSdfOutline();

                // This is needed to 'fill' the shadow body since
                // we have no good way of knowing if the pixel is inside or outside
                // of the shadow limit
                if (distance_to_edge_normalized > sdf_outline)
                {
                	distance_to_border = edge;
                }

                // Calculate shadow distribution separately in a different channel since we spread the
                // values across the distance to the outline border and not to the edge.
                float distance_to_border_normalized = calculateSdfEdgeLimit((float)distance_to_border, sdf_shadow_spread);

                // Map outgoing distance value to 0..255
                int shadow_channel = (int)(255.0f * distance_to_border_normalized);
                shadow_channel     = Math.max(0,Math.min(255,shadow_channel));

                image.setRGB(u, v, 0x010000 * outline_channel | 0x000001 * shadow_channel);
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
                    int edge_outline_channel = image.getRGB(u,v);
                    int shadow_channel = blurredShadowImage.getRGB(u,v);

                    image.setRGB(u,v,edge_outline_channel & 0xFFFF00 | shadow_channel & 0xFF);
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

        if (this.fontDesc.getAntialias() != 0) {
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
        } else {
            g.setPaint(faceColor);
            g.setFont(font);
            g.drawString(new String(Character.toChars(glyph.c)), 0, 0);
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

    private int getFontMapLayerMask()
    {
        int fontMapLayerMask = LAYER_FACE;

        if (this.fontDesc.getRenderMode() == FontRenderMode.MODE_MULTI_LAYER)
        {
            if (this.fontDesc.getAntialias() != 0 &&
                this.fontDesc.getOutlineAlpha() > 0 &&
                this.fontDesc.getOutlineWidth() > 0)
            {
                fontMapLayerMask |= LAYER_OUTLINE;
            }

            if (this.fontDesc.getAntialias() != 0 &&
                this.fontDesc.getShadowAlpha() > 0 &&
                this.fontDesc.getAlpha() > 0)
            {
                fontMapLayerMask |= LAYER_SHADOW;
            }
        }

        return fontMapLayerMask;
    }

    public BufferedImage compile(InputStream fontStream, FontDesc fontDesc, boolean preview, final FontResourceResolver resourceResolver) throws FontFormatException, IOException {
        this.fontDesc = fontDesc;
        this.fontMapBuilder = FontMap.newBuilder();

        if (fontDesc.getFont().toLowerCase().endsWith("fnt")) {
            FNTBuilder(fontStream);
        } else {
            TTFBuilder(fontStream);
        }
        fontMapBuilder.setMaterial(fontDesc.getMaterial() + "c");
        fontMapBuilder.setImageFormat(fontDesc.getOutputFormat());

        return generateGlyphData(preview, resourceResolver);
    }

    public FontMap getFontMap() {
        return fontMapBuilder.build();
    }

    public BufferedImage generatePreviewImage() throws IOException {

        Graphics2D g;
        BufferedImage previewImage = new BufferedImage(fontMapBuilder.getCacheWidth(), fontMapBuilder.getCacheHeight(), BufferedImage.TYPE_3BYTE_BGR);
        g = previewImage.createGraphics();
        g.setBackground(Color.BLACK);
        g.clearRect(0, 0, previewImage.getWidth(), previewImage.getHeight());
        setHighQuality(g);

        int cache_columns = fontMapBuilder.getCacheWidth() / fontMapBuilder.getCacheCellWidth();
        int cache_rows = fontMapBuilder.getCacheHeight() / fontMapBuilder.getCacheCellHeight();

        BufferedImage glyphImage;
        for (int i = 0; i < glyphs.size(); i++) {

            Glyph glyph = glyphs.get(i);

            int col = i % cache_columns;
            int row = i / cache_columns;

            if (row >= cache_rows) {
                break;
            }

            int x = col * fontMapBuilder.getCacheCellWidth();
            int y = row * fontMapBuilder.getCacheCellHeight();

            glyph.x = x;
            glyph.y = y;

            g.translate(x, y);
            glyphImage = glyph.image;
            g.drawImage(glyphImage, 0, 0, null);
            g.translate(-x, -y);

        }

        return previewImage;
    }

    public static void main(String[] args) throws FontFormatException {
        try {
            System.setProperty("java.awt.headless", "true");
            if (args.length != 2 && args.length != 3 && args.length != 4)    {
                System.err.println("Usage: fontc fontfile [basedir] outfile");
                System.exit(1);
            }

            String basedir = ".";
            String outfile = args[1];
            if (args.length >= 3) {
                basedir = args[1];
                outfile = args[2];
            }
            final File fontInput = new File(args[0]);
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
            Fontc fontc = new Fontc();
            String fontInputFile = basedir + File.separator + fontDesc.getFont();
            BufferedInputStream fontInputStream = new BufferedInputStream(new FileInputStream(fontInputFile));
            fontc.compile(fontInputStream, fontDesc, false, new FontResourceResolver() {

                @Override
                public InputStream getResource(String resourceName) throws FileNotFoundException {
                    Path resPath = Paths.get(fontInput.getParent().toString(), resourceName);
                    BufferedInputStream resStream = new BufferedInputStream(new FileInputStream(resPath.toString()));

                    return resStream;
                }
            });
            fontInputStream.close();

            // Write fontmap file
            FileOutputStream fontMapOutputStream = new FileOutputStream(outfile);
            fontc.getFontMap().writeTo(fontMapOutputStream);
            fontMapOutputStream.close();

        } catch (IOException e) {
            System.err.println(e.getMessage());
            System.exit(1);
        }
    }
}
