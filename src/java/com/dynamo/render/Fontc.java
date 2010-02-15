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

class Glyph implements Comparable<Glyph>
{
    char m_C;
    int m_X;
    int m_Y;
    int m_Width;
    int m_Height;
    int m_YOffset;
    Shape m_Shape;
    Rectangle m_VisualBounds;
    int m_Index;
    int m_LeftBearing;
    int m_RightBearing;
    
    @Override
    public int compareTo(Glyph o)
    {
        Integer a = m_Height;
        Integer b = o.m_Height;
        return b.compareTo(a);
    }
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

        if (true)
        {
            image_g.setRenderingHint(RenderingHints.KEY_ALPHA_INTERPOLATION, RenderingHints.VALUE_ALPHA_INTERPOLATION_QUALITY);
            image_g.setRenderingHint(RenderingHints.KEY_COLOR_RENDERING, RenderingHints.VALUE_COLOR_RENDER_QUALITY);
            image_g.setRenderingHint(RenderingHints.KEY_DITHERING, RenderingHints.VALUE_DITHER_DISABLE);
            image_g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BICUBIC);
            image_g.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);

            image_g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            image_g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL, RenderingHints.VALUE_STROKE_PURE);
            image_g.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
            image_g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        }

        ArrayList<Glyph> glyphs = new ArrayList<Glyph>();
        for (int i = 0; i < m_Characters.length(); ++i)
        {
            String s = m_Characters.substring(i, i+1);

            GlyphVector glyph_vector = font.createGlyphVector(m_FontRendererContext, s);
            Shape shape = glyph_vector.getOutline(0, 0);
            Rectangle visual_bounds = glyph_vector.getGlyphVisualBounds(0).getBounds();
            GlyphMetrics metrics = glyph_vector.getGlyphMetrics(0);
                
            Glyph glyph = new Glyph();
            glyph.m_C = s.charAt(0);
            glyph.m_Width = visual_bounds.width;
            glyph.m_Height = visual_bounds.height;
            glyph.m_Shape = shape;
            glyph.m_VisualBounds = visual_bounds;
            glyph.m_Index = i;
            glyph.m_LeftBearing = (int) metrics.getLSB();
            glyph.m_RightBearing = Math.round(metrics.getRSB() + (glyph.m_LeftBearing - metrics.getLSB()));

            if (glyph.m_Width == 0)
            {
                glyph.m_Width = (int) (metrics.getAdvance());
            }

            TextLayout layout = new TextLayout(s, font, m_FontRendererContext);
            glyph.m_YOffset = (int) layout.getAscent();

            glyphs.add(glyph);
        }
        Collections.sort(glyphs);

        Rectangle2D max_bounds = font.getMaxCharBounds(m_FontRendererContext);
        int max_layout_width = m_ImageWidth-((int) max_bounds.getWidth());
            
        int i = 0;
        int y = 0;
        int margin = 4;
        while (i < m_Characters.length())
        {
            int x = 0;
            int max_y = 0;
            int j = i;
            while (j < m_Characters.length() && x < max_layout_width )
            {
                Glyph g = glyphs.get(j);
                max_y = Math.max(max_y, g.m_Height);
                x += g.m_Width + margin;
                ++j;
            }

            x = 0;
            for (int k = i; k < j; ++k)
            {
                Glyph g = glyphs.get(k);

                int dx = x - g.m_VisualBounds.x;
                int dy = y - g.m_VisualBounds.y;
                g.m_X = x;
                g.m_Y = y;

                image_g.setPaint(Color.white);
                image_g.translate(dx, dy);
                image_g.fill(g.m_Shape);

                image_g.translate(-dx, -dy);
                x += g.m_Width + margin;
            }
            y += max_y + margin;
            i = j;
        }

        int new_height = (int) (Math.log(y) / Math.log(2));
        new_height = (int) Math.pow(2, new_height + 1);
        image = image.getSubimage(0, 0, m_ImageWidth, new_height);

        Collections.sort(glyphs, new OrderComparator());

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
            image_glyph.m_X = g.m_X;
            image_glyph.m_Y = g.m_Y;
            image_glyph.m_Width = g.m_Width;
            image_glyph.m_Height = g.m_Height;
            image_glyph.m_YOffset = g.m_VisualBounds.y;
            image_font.m_Glyphs.add(image_glyph);
            image_glyph.m_LeftBearing = g.m_LeftBearing;
            image_glyph.m_RightBearing = g.m_RightBearing;
            
            if (g.m_C == ' ')
            {
                // Skip for ' '. Currently incorrect
                image_glyph.m_LeftBearing = 0;
                image_glyph.m_RightBearing = 0;                
            }
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
