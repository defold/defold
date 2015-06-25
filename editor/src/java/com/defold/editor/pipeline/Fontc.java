package com.defold.editor.pipeline;

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
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Comparator;

import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontTextureFormat;
import com.google.protobuf.ByteString;
import com.google.protobuf.TextFormat;

class Glyph {
    int index;
    char c;
    int width;
    int advance;
    int leftBearing;
    int ascent;
    int descent;
    int x;
    int y;
    GlyphVector vector;
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
    int imageWidth = 1024;
    int imageHeight = 2048; // Enough. Will be cropped later
    Stroke outlineStroke = null;
    private StringBuffer characters;
    private FontRenderContext fontRendererContext;
    private BufferedImage image;
    private FontDesc fontDesc;
    static final int imageType = BufferedImage.TYPE_3BYTE_BGR;
    static final int imageComponentCount = 3;

    public Fontc() {
    }

    public void run(InputStream fontStream, FontDesc fontDesc, OutputStream fontMapOutput) throws FontFormatException, IOException {
        this.characters = new StringBuffer();
        // 7-bit ASCII. Note inclusive range [32,126]
        for (int i = 32; i <= 126; ++i)
            this.characters.append((char) i);

        String extraCharacters = fontDesc.getExtraCharacters();
        for (int i = 0; i < extraCharacters.length(); i++) {
            char c = extraCharacters.charAt(i);
            this.characters.append(c);
        }

        this.fontDesc = fontDesc;
        this.fontRendererContext = new FontRenderContext(new AffineTransform(), fontDesc.getAntialias() != 0, fontDesc.getAntialias() != 0);

        if (fontDesc.getOutlineWidth() > 0.0f) {
            outlineStroke = new BasicStroke(fontDesc.getOutlineWidth());
        }

        Font font = Font.createFont(Font.TRUETYPE_FONT, fontStream);
        font = font.deriveFont(Font.PLAIN, fontDesc.getSize());
        for (int i = 0; i < this.characters.length(); ++i) {
            char ch = this.characters.charAt(i);
            if (!font.canDisplay(ch)) {
                this.characters.deleteCharAt(i);
                i--;
            }
        }

        image = new BufferedImage(this.imageWidth, this.imageHeight, Fontc.imageType);
        Graphics2D g = image.createGraphics();
        g.setBackground(this.fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD ? Color.WHITE : Color.BLACK);
        g.clearRect(0, 0, image.getWidth(), image.getHeight());
        setHighQuality(g);

        FontMetrics fontMetrics = g.getFontMetrics(font);
        int maxAscent = fontMetrics.getMaxAscent();
        int maxDescent = fontMetrics.getMaxDescent();

        ArrayList<Glyph> glyphs = new ArrayList<Glyph>();
        for (int i = 0; i < this.characters.length(); ++i) {
            String s = this.characters.substring(i, i+1);

            GlyphVector glyphVector = font.createGlyphVector(this.fontRendererContext, s);
            Rectangle visualBounds = glyphVector.getOutline().getBounds();
            GlyphMetrics metrics = glyphVector.getGlyphMetrics(0);

            Glyph glyph = new Glyph();
            glyph.ascent = (int)Math.ceil(-visualBounds.getMinY());
            glyph.descent = (int)Math.ceil(visualBounds.getMaxY());

            glyph.c = s.charAt(0);
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


        int i = 0;
        float totalY = 0.0f;
        // Margin is set to 1 since the font map is an atlas, this removes sampling artifacts (neighbouring texels outside the sample-area being used)
        int margin = 1;
        int padding = 0;
        float sdf_scale = 0;
        float sdf_offset = 0;
        if (this.fontDesc.getAntialias() != 0)
            padding = Math.min(4, this.fontDesc.getShadowBlur()) + (int)Math.ceil(this.fontDesc.getOutlineWidth() * 0.5f);
        if (this.fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {
            padding++; // to give extra range for the outline.
            // need sqrt(2) on either side of the range [0, outlineWidth + 1] to prevent clamped values being used
            // for interpolation across texels in the output. The extra is for smoothstep room.
            float sqrt2 = 1.4142f;
            sdf_scale = 1.0f / (1 + 2.0f * sqrt2 + this.fontDesc.getOutlineWidth());
            sdf_offset = sdf_scale * sqrt2; // where glyph ends
        }
        Color faceColor = new Color(this.fontDesc.getAlpha(), 0.0f, 0.0f);
        Color outlineColor = new Color(0.0f, this.fontDesc.getOutlineAlpha(), 0.0f);
        ConvolveOp shadowConvolve = null;
        Composite blendComposite = new BlendComposite();
        if (this.fontDesc.getShadowAlpha() > 0.0f) {
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
        while (i < this.characters.length()) {
            float x = margin;
            float maxY = 0.0f;
            int j = i;
            while (j < this.characters.length() && x < this.imageWidth ) {
                Glyph glyph = glyphs.get(j);
                int width = glyph.width + margin + padding * 2;
                if (x + width < this.imageWidth) {
                    maxY = Math.max(maxY, glyph.ascent + glyph.descent);
                    x += width;
                } else {
                    break;
                }
                ++j;
            }

            g.translate(0, margin);

            for (int k = i; k < j; ++k) {
                Glyph glyph = glyphs.get(k);

                g.translate(margin, 0);

                if (glyph.width > 0.0f) {
                    BufferedImage glyphImage;
                    if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP)
                        glyphImage = drawGlyph(glyph, padding, font, blendComposite, faceColor, outlineColor, shadowConvolve);
                    else
                        glyphImage = makeDistanceField(glyph, padding, sdf_scale, sdf_offset, font);
                    g.drawImage(glyphImage, 0, 0, null);
                    glyph.x += g.getTransform().getTranslateX();
                    glyph.y += g.getTransform().getTranslateY();

                    g.translate(glyphImage.getWidth(), 0);
                }
            }
            g.translate(-g.getTransform().getTranslateX(), maxY + padding * 2);
            totalY += maxY + margin + 2 * padding;
            i = j;
        }
        totalY += margin;

        int newHeight = (int) (Math.log(totalY) / Math.log(2));
        newHeight = (int) Math.pow(2, newHeight + 1);
        image = image.getSubimage(0, 0, this.imageWidth, newHeight);

        FontMap.Builder builder = FontMap.newBuilder()
            .setMaterial(fontDesc.getMaterial() + "c")
            .setImageWidth(this.imageWidth)
            .setImageHeight(newHeight)
            .setShadowX(this.fontDesc.getShadowX())
            .setShadowY(this.fontDesc.getShadowY())
            .setMaxAscent(maxAscent)
            .setMaxDescent(maxDescent)
            .setImageFormat(this.fontDesc.getOutputFormat());

        if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {
            // sdf_scale has up until now been the value to scale with.
            // Now recompute their inverses to be used for unpacking it.
            builder.setSdfScale(1.0f / sdf_scale);
            builder.setSdfOffset(-sdf_offset / sdf_scale);
            builder.setSdfOutline(this.fontDesc.getOutlineWidth());
        }

        i = 0;
        for (Glyph glyph : glyphs) {
            FontMap.Glyph.Builder glyphBuilder = FontMap.Glyph.newBuilder()
                .setCharacter(glyph.c)
                .setWidth(glyph.width + (glyph.width > 0 ? padding * 2 : 0))
                .setAdvance(glyph.advance)
                .setLeftBearing(glyph.leftBearing - padding)
                .setAscent(glyph.ascent + padding)
                .setDescent(glyph.descent + padding)
                .setX(glyph.x)
                .setY(glyph.y);
            builder.addGlyphs(glyphBuilder);
        }

        int imageSize = this.imageWidth * newHeight * Fontc.imageComponentCount;
        int[] imageData = new int[imageSize];
        byte[] imageBytes = new byte[imageSize];
        image.getRaster().getPixels(0, 0, this.imageWidth, newHeight, imageData);

        for (int j = 0; j < imageSize; ++j) {
            imageBytes[j] = (byte)(imageData[j] & 0xff);
        }
        builder.setImageData(ByteString.copyFrom(imageBytes));

        if (fontMapOutput != null) {
            builder.build().writeTo(fontMapOutput);
        }
    }

    private BufferedImage makeDistanceField(Glyph glyph, int padding, float sdf_scale, float sdf_offset, Font font) {
        int width = glyph.width + padding * 2;
        int height = glyph.ascent + glyph.descent + padding * 2;

        Shape sh = glyph.vector.getGlyphOutline(0);
        PathIterator pi = sh.getPathIterator(new AffineTransform(1,0,0,1,0,0));
        pi = new FlatteningPathIterator(pi,  0.1);

        double _x = 0, _y = 0;
        DistanceFieldGenerator df = new DistanceFieldGenerator();
        while (!pi.isDone()) {
            double [] c = new double[100];
            int res = pi.currentSegment(c);
            switch (res) {
              case PathIterator.SEG_MOVETO:
                  _x = c[0];
                  _y = c[1];
                  break;
              case PathIterator.SEG_LINETO:
                  df.addLine(_x, _y, c[0], c[1]);
                  _x = c[0];
                  _y = c[1];
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

        BufferedImage image = new BufferedImage(width, height, Fontc.imageType);
        for (int v=0;v<height;v++) {
            int ofs = v * width;
            for (int u=0;u<width;u++) {
                double gx = u0 + kx * u * (u1 - u0);
                double gy = v0 + ky * v * (v1 - v0);
                double value = res[ofs + u];
                if (sh.contains(gx, gy))
                    value = -value;
                int oval = (int)(255 * (value * sdf_scale + sdf_offset));
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

        BufferedImage image = new BufferedImage(width, height, Fontc.imageType);
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
            g.drawString(Character.toString(glyph.c), 0, 0);
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

    public static BufferedImage compileToImage(InputStream fontStream, FontDesc fontDesc) throws FontFormatException, IOException {
        Fontc fontc = new Fontc();
        fontc.run(fontStream, fontDesc, null);
        return fontc.image;
    }

    public static void main(String[] args) throws FontFormatException {
        try {
            System.setProperty("java.awt.headless", "true");
            if (args.length != 2 && args.length != 3)    {
                System.err.println("Usage: fontc fontfile [basedir] outfile");
                System.exit(1);
            }
            String basedir = ".";
            String outfile = args[1];
            if (args.length == 3) {
                basedir = args[1];
                outfile = args[2];
            }
            File fontInput = new File(args[0]);
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

            Fontc fontc = new Fontc();
            String fontFile = basedir + File.separator + fontDesc.getFont();
            BufferedInputStream fontStream = new BufferedInputStream(new FileInputStream(fontFile));
            FileOutputStream fontMapOutput = null;
            if (outfile != null) {
            	fontMapOutput = new FileOutputStream(new File(outfile));
            }
            fontc.run(fontStream, fontDesc, fontMapOutput);
            if (fontMapOutput != null) {
            	fontMapOutput.close();
            }
            fontStream.close();
        }
        catch (IOException e) {
            System.err.println(e.getMessage());
            System.exit(1);
        }
    }
}
