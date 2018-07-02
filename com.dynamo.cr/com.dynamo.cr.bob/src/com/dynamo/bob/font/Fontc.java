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

    public BufferedImage generateGlyphData(boolean preview, final FontResourceResolver resourceResolver) throws FontFormatException {

        ByteArrayOutputStream glyphDataBank = new ByteArrayOutputStream(1024*1024*4);

        // Padding is the pixel amount needed to get a good antialiasing around the glyphs, while cell padding
        // is the extra padding added to the bitmap data to avoid filtering glitches when rendered.
        int padding = 0;
        int cell_padding = 1;
        float sdf_spread = 0.0f;
        float edge = 0.75f;
        if (fontDesc.getAntialias() != 0)
            padding = Math.min(4, fontDesc.getShadowBlur()) + (int)(fontDesc.getOutlineWidth());
        if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {
            padding++; // to give extra range for the outline.

            // Make sure the outline edge is not zero which would cause everything outside the edge becoming outline.
            // We use sqrt(2) since it is the diagonal length of a pixel, but any small positive value would do.
            float sqrt2 = 1.4142f;
            sdf_spread = sqrt2 + fontDesc.getOutlineWidth();
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
            fontMapBuilder.setSdfSpread(sdf_spread);
            
            // Transform outline edge from pixel unit to edge offset in distance field unit
            float outline_edge = -(fontDesc.getOutlineWidth() / (sdf_spread)); // Map to [-1, 1]
            outline_edge = outline_edge * (1.0f - edge) + edge; // Map to edge distribution
            fontMapBuilder.setSdfOutline(outline_edge);
            
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
            channelCount = 1;
        }

        // We keep track of offset into the glyph data bank,
        // this is saved for each glyph to know where their bitmap data is stored.
        int dataOffset = 0;

        // Find max width, height for each glyph to lock down cache cell sizes,
        // this includes cell padding.
        int cell_width = 0;
        int cell_height = 0;
        for (int i = 0; i < glyphs.size(); i++) {
            Glyph glyph = glyphs.get(i);
            if (glyph.width <= 0.0f) {
                continue;
            }
            int width = glyph.width + padding * 2 + cell_padding * 2;
            int height = glyph.ascent + glyph.descent + padding * 2 + cell_padding * 2;
            cell_width = Math.max(cell_width, width);
            cell_height = Math.max(cell_height, height);
        }

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
                glyphImage = makeDistanceField(glyph, padding, sdf_spread, font, edge);
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
                for (int y = 0; y < glyphImage.getHeight(); y++) {

                    if (y == 0) {
                        repeatedWrite(glyphDataBank, (glyphImage.getWidth() + cell_padding * 2) * channelCount, clearData);
                    }
                    for (int x = 0; x < glyphImage.getWidth(); x++) {

                        if (x == 0) {
                            repeatedWrite(glyphDataBank, channelCount, clearData);
                        }

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

                        if (x == glyphImage.getWidth()-1) {
                            repeatedWrite(glyphDataBank, channelCount, clearData);
                        }

                    }
                    if (y == glyphImage.getHeight()-1) {
                        repeatedWrite(glyphDataBank, (glyphImage.getWidth() + cell_padding * 2) * channelCount, clearData);
                    }
                }
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

    private BufferedImage makeDistanceField(Glyph glyph, int padding, float sdf_spread, Font font, float edge) {
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

        double[] res = new double[width*height];
        df.render(res, u0, v0, u1, v1, width, height);
        double kx = 1 / (double)width;
        double ky = 1 / (double)height;

        BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_3BYTE_BGR);
        for (int v=0;v<height;v++) {
            int ofs = v * width;
            for (int u=0;u<width;u++) {
                double gx = u0 + kx * u * (u1 - u0);
                double gy = v0 + ky * v * (v1 - v0);
                double value = res[ofs + u];
                if (!sh.contains(gx, gy)) {
                    value = -value;
                }
                // Transform distance from pixel unit to edge-relative distance field unit
                float df_norm = (float) ((value / sdf_spread)); // Map to [-1, 1]
                df_norm = df_norm * (1.0f - edge) + edge; // Map to edge distribution [0, 1]
                int oval = (int)(255.0f * df_norm); // Map to [0, 255]

                if (oval < 0) {
                    oval = 0;
                } else if (oval > 255) {
                    oval = 255;
                }
                image.setRGB(u, v, 0x10101 * oval);
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
