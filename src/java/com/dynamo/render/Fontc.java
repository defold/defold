package com.dynamo.render;

import java.awt.Color;
import java.awt.Font;
import java.awt.FontFormatException;
import java.awt.Graphics2D;
import java.awt.Rectangle;
import java.awt.RenderingHints;
import java.awt.Shape;
import java.awt.font.FontRenderContext;
import java.awt.font.GlyphMetrics;
import java.awt.font.GlyphVector;
import java.awt.font.TextLayout;
import java.awt.geom.AffineTransform;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;

import javax.imageio.ImageIO;

import com.dynamo.ddf.DDF;
import com.dynamo.render.ddf.Font.ImageFont;

class Glyph
{
    int m_Index;
    char m_C;
    int m_Width;
    int m_Advance;
    int m_LeftBearing;
    int m_Ascent;
    int m_Descent;
    int m_X;
    int m_Y;
    GlyphVector m_Vector;
};

class OrderComparator implements Comparator<Glyph>
{
    @Override
    public int compare(Glyph o1, Glyph o2)
    {
        return (new Integer(o1.m_Index)).compareTo(o2.m_Index);
    }
}

public class Fontc
{
    String m_Fontfile = null;
    int m_Fontsize = -1;
    String m_ImagefontFile = null;
    int m_ImageWidth;
    int m_ImageHeight;
    private StringBuffer m_Characters;
    private FontRenderContext m_FontRendererContext;

    public Fontc()
    {
        m_Characters = new StringBuffer();
        for (int i = 32; i < 126; ++i)
            m_Characters.append((char) i);

        m_FontRendererContext = new FontRenderContext(new AffineTransform(), true, true);
    }

    @SuppressWarnings("unchecked")
    public void run() throws FontFormatException, IOException
    {
        Font font = Font.createFont(Font.TRUETYPE_FONT, new File(m_Fontfile));
        font = font.deriveFont(Font.PLAIN, m_Fontsize);

        BufferedImage image = new BufferedImage(m_ImageWidth, m_ImageHeight, BufferedImage.TYPE_BYTE_GRAY);
        Graphics2D image_g = image.createGraphics();
        image_g.setPaint(Color.white);

        ArrayList<Glyph> glyphs = new ArrayList<Glyph>();
        for (int i = 0; i < m_Characters.length(); ++i)
        {
            String s = m_Characters.substring(i, i+1);

            GlyphVector glyph_vector = font.createGlyphVector(m_FontRendererContext, s);
            Rectangle visual_bounds = glyph_vector.getOutline().getBounds();
            GlyphMetrics metrics = glyph_vector.getGlyphMetrics(0);

            Glyph glyph = new Glyph();

            TextLayout layout = new TextLayout(s, font, m_FontRendererContext);
            glyph.m_Ascent = (int)Math.ceil(layout.getAscent());
            glyph.m_Descent = (int)Math.ceil(layout.getDescent());

            glyph.m_C = s.charAt(0);
            glyph.m_Index = i;
            glyph.m_Advance = (int)Math.ceil(metrics.getAdvance());
            glyph.m_LeftBearing = (int)Math.floor(metrics.getLSB());
            glyph.m_Width = visual_bounds.width;

            glyph.m_Vector = glyph_vector;

            glyphs.add(glyph);
        }

        int i = 0;
        float y = 0.0f;
        int margin = 0;
        while (i < m_Characters.length())
        {
            float x = 0;
            float max_y = 0.0f;
            int j = i;
            while (j < m_Characters.length() && x < m_ImageWidth )
            {
                Glyph g = glyphs.get(j);
                if (x + g.m_Width + margin < m_ImageWidth)
                {
                    max_y = Math.max(max_y, g.m_Ascent + g.m_Descent + margin);
                    x += g.m_Width + margin;
                }
                else
                {
                    break;
                }
                ++j;
            }

            image_g.translate(0, margin);

            for (int k = i; k < j; ++k)
            {
                Glyph g = glyphs.get(k);

                image_g.translate(margin, 0);

                double dx = -g.m_LeftBearing;
                double dy = g.m_Ascent;

                image_g.translate(dx, dy);

                image_g.drawGlyphVector(g.m_Vector, 0, 0);
                g.m_X = (int)Math.round(image_g.getTransform().getTranslateX());
                g.m_Y = (int)Math.round(image_g.getTransform().getTranslateY());

                image_g.translate(-dx, -dy);

                image_g.translate(g.m_Width, 0);
            }
            image_g.translate(-image_g.getTransform().getTranslateX(), max_y - margin);
            y += max_y + margin;
            i = j;
        }

        int new_height = (int) (Math.log(y) / Math.log(2));
        new_height = (int) Math.pow(2, new_height + 1);
        image = image.getSubimage(0, 0, m_ImageWidth, new_height);

        ImageFont image_font = new ImageFont();
        image_font.m_ImageWidth = m_ImageWidth;
        image_font.m_ImageHeight = new_height;

        // Add 32 dummy characters
        for (int j = 0; j < 32; ++j)
        {
            com.dynamo.render.ddf.Font.ImageFont.Glyph image_glyph = new com.dynamo.render.ddf.Font.ImageFont.Glyph();
            image_font.m_Glyphs.add(image_glyph);
        }

        i = 0;
        for (Glyph g : glyphs)
        {
            com.dynamo.render.ddf.Font.ImageFont.Glyph image_glyph = new com.dynamo.render.ddf.Font.ImageFont.Glyph();
            image_glyph.m_Width = g.m_Width;
            image_glyph.m_Advance = g.m_Advance;
            image_glyph.m_LeftBearing = g.m_LeftBearing;
            image_glyph.m_Ascent = g.m_Ascent;
            image_glyph.m_Descent = g.m_Descent;
            image_glyph.m_X = g.m_X;
            image_glyph.m_Y = g.m_Y;
            image_font.m_Glyphs.add(image_glyph);
        }

        image_font.m_ImageData = new byte[m_ImageWidth * new_height];
        int[] image_data = new int[m_ImageWidth * new_height];
        image.getRaster().getPixels(0, 0, m_ImageWidth, new_height, image_data);

        for (int j = 0; j < m_ImageWidth * new_height; ++j)
        {
            image_font.m_ImageData[j] = (byte) (image_data[j] & 0xff);
        }

        ImageIO.write(image, "png", new File(m_ImagefontFile + ".png"));
        DDF.save(image_font, new FileOutputStream(m_ImagefontFile));
    }

    public static void main(String[] args) throws FontFormatException, IOException
    {
        System.setProperty("java.awt.headless", "true");
        if (args.length != 3)
        {
            System.err.println("Usage: fontc fontfile size outfile");
            System.exit(1);
        }
        Fontc fontc = new Fontc();
        fontc.m_Fontfile = args[0];
        fontc.m_Fontsize = Integer.parseInt(args[1]);
        fontc.m_ImagefontFile = args[2];
        fontc.m_ImageWidth = 512;
        fontc.m_ImageHeight = 2048; // Enough. Will be cropped later

        fontc.run();
    }
}
