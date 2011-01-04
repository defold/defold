/*******************************************************************************
 * Copyright (c) 2006 Chris Gross. All rights reserved. This program and the
 * accompanying materials are made available under the terms of the Eclipse
 * Public License v1.0 which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html Contributors: schtoo@schtoo.com
 * (Chris Gross) - initial API and implementation
 ******************************************************************************/

package org.eclipse.nebula.widgets.pgroup.internal;

import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.widgets.Display;

/**
 * @author chris
 */
public class GraphicUtils
{

    private static void drawRoundCorner(GC gc, int x, int y, Color outerColor, Color borderColor,
                                        Color innerColor, boolean top, boolean left)
    {

        Color fore = gc.getForeground();

        int corner[][] = null;

        if (top && !left)
        {
            int i[][] = { {0, 0, 0, 0, 0 }, {1, 1, 0, 0, 0 }, {2, 2, 1, 0, 0 }, {2, 2, 2, 1, 0 },
                         {2, 2, 2, 1, 0 } };
            corner = i;
        }
        else if (!top && left)
        {
            int i[][] = { {0, 1, 2, 2, 2 }, {0, 1, 2, 2, 2 }, {0, 0, 1, 2, 2 }, {0, 0, 0, 1, 1 },
                         {0, 0, 0, 0, 0 } };
            corner = i;
        }
        else if (!top && !left)
        {
            int i[][] = { {2, 2, 2, 1, 0 }, {2, 2, 2, 1, 0 }, {2, 2, 1, 0, 0 }, {1, 1, 0, 0, 0 },
                         {0, 0, 0, 0, 0 } };
            corner = i;
        }
        else
        {
            int i[][] = { {0, 0, 0, 0, 0 }, {0, 0, 0, 1, 1 }, {0, 0, 1, 2, 2 }, {0, 1, 2, 2, 2 },
                         {0, 1, 2, 2, 2 } };
            corner = i;
        }

        // one pass for each color
        for (int i = 0; i < 3; i++)
        {
            if (i == 0)
            {
                if (outerColor == null)
                    continue;
                gc.setForeground(outerColor);
            }
            if (i == 1)
            {
                if (borderColor == null)
                    continue;
                gc.setForeground(borderColor);
            }
            if (i == 2)
            {
                if (innerColor == null)
                    continue;
                gc.setForeground(innerColor);
            }

            for (int line = 0; line < 5; line++)
            {
                for (int x2 = 0; x2 < 5; x2++)
                {
                    if (corner[line][x2] == i)
                        gc.drawPoint(x + x2, y + line);
                }
            }
        }

        gc.setForeground(fore);
    }

    public static void drawRoundRectangle(GC gc, int x, int y, int width, int height,
                                          Color outerColor)
    {
        drawRoundRectangle(gc, x, y, width, height, outerColor, true, true);
    }

    public static void fillGradientRectangle(GC gc, int x, int y, int width, int height,
                                             Color[] gradientColors, int[] gradientPercents,
                                             boolean vertical)
    {
        final Color oldBackground = gc.getBackground();
        if (gradientColors.length == 1)
        {
            if (gradientColors[0] != null)
                gc.setBackground(gradientColors[0]);
            gc.fillRectangle(x, y, width, height);
        }
        else
        {
            final Color oldForeground = gc.getForeground();
            Color lastColor = gradientColors[0];
            if (lastColor == null)
                lastColor = oldBackground;
            int pos = 0;
            for (int i = 0; i < gradientPercents.length; ++i)
            {
                gc.setForeground(lastColor);
                lastColor = gradientColors[i + 1];
                if (lastColor == null)
                    lastColor = oldBackground;
                gc.setBackground(lastColor);
                if (vertical)
                {
                    final int gradientHeight = (gradientPercents[i] * height / 100) - pos;
                    gc.fillGradientRectangle(x, y + pos, width, gradientHeight, true);
                    pos += gradientHeight;
                }
                else
                {
                    final int gradientWidth = (gradientPercents[i] * width / 100) - pos;
                    gc.fillGradientRectangle(x + pos, y, gradientWidth, height, false);
                    pos += gradientWidth;
                }
            }
            if (vertical && pos < height)
            {
                gc.setBackground(oldBackground);
                gc.fillRectangle(x, y + pos, width, height - pos);
            }
            if (!vertical && pos < width)
            {
                gc.setBackground(oldBackground);
                gc.fillRectangle(x + pos, y, width - pos, height);
            }
            gc.setForeground(oldForeground);
        }
        gc.setBackground(oldBackground);
    }

    public static void drawRoundRectangle(GC gc, int x, int y, int width, int height,
                                          Color outerColor, Color borderColor, boolean roundTop,
                                          boolean roundBottom)
    {

        if (borderColor != null)
        {
            Color fore = gc.getForeground();
            gc.setForeground(borderColor);
            gc.drawRectangle(x, y, width, height - 1);
            gc.setForeground(fore);
        }

        if (roundTop)
        {
            drawRoundCorner(gc, x, y, outerColor, borderColor, null, true, true);
            drawRoundCorner(gc, x + width - 4, y, outerColor, borderColor, null, true, false);
        }
        if (roundBottom)
        {
            drawRoundCorner(gc, x, y + height - 5, outerColor, borderColor, null, false, true);
            drawRoundCorner(gc, x + width - 4, y + height - 5, outerColor, borderColor, null,
                            false, false);
        }
    }

    public static void drawRoundRectangle(GC gc, int x, int y, int width, int height,
                                          Color outerColor, boolean roundTop, boolean roundBottom)
    {
        drawRoundRectangle(gc, x, y, width, height, outerColor, gc.getForeground(), roundTop,
                           roundBottom);
    }

    public static void fillRoundRectangle(GC gc, int x, int y, int width, int height,
                                          Color outerColor)
    {
        fillRoundRectangle(gc, x, y, width, height, outerColor, true, true);
    }

    public static void fillRoundRectangle(GC gc, int x, int y, int width, int height,
                                          Color outerColor, boolean roundTop, boolean roundBottom)
    {

        gc.fillRectangle(x, y, width, height);

        if (roundTop)
        {
            drawRoundCorner(gc, x, y, outerColor, gc.getBackground(), gc.getBackground(), true,
                            true);
            drawRoundCorner(gc, x + width - 5, y, outerColor, gc.getBackground(), gc
                .getBackground(), true, false);
        }
        if (roundBottom)
        {
            drawRoundCorner(gc, x, y + height - 5, outerColor, gc.getBackground(), gc
                .getBackground(), false, true);
            drawRoundCorner(gc, x + width - 5, y + height - 5, outerColor, gc.getBackground(), gc
                .getBackground(), false, false);
        }
    }

    public static int blend(int v1, int v2, int ratio)
    {
        return (ratio * v1 + (100 - ratio) * v2) / 100;
    }

    public static RGB blend(RGB c1, RGB c2, int ratio)
    {
        int r = blend(c1.red, c2.red, ratio);
        int g = blend(c1.green, c2.green, ratio);
        int b = blend(c1.blue, c2.blue, ratio);
        return new RGB(r, g, b);
    }

    public static Color createNewBlendedColor(RGB rgb1, RGB rgb2, int ratio)
    {

        Color newColor = new Color(Display.getCurrent(), blend(rgb1, rgb2, ratio));

        return newColor;

    }

    public static Color createNewBlendedColor(Color c1, Color c2, int ratio)
    {

        Color newColor = new Color(Display.getCurrent(), blend(c1.getRGB(), c2.getRGB(), ratio));

        return newColor;
    }

    public static Color createNewReverseColor(Color c)
    {
        Color newColor = new Color(Display.getCurrent(), 255 - c.getRed(), 255 - c.getGreen(),
                                   255 - c.getBlue());
        return newColor;
    }

    public static RGB saturate(RGB rgb, float saturation)
    {
        float[] hsb = java.awt.Color.RGBtoHSB(rgb.red, rgb.green, rgb.blue, null);

        hsb[1] += saturation;
        if (hsb[1] > 1.0f)
            hsb[1] = 1.0f;
        if (hsb[1] < 0f)
            hsb[1] = 0f;

        hsb[0] += saturation;
        if (hsb[0] > 1.0f)
            hsb[0] = 1.0f;

        if (hsb[0] < 0f)
            hsb[0] = 0f;

        // hsb[2] += saturation;
        // if (hsb[2] > 1.0f)
        // hsb[2] = 1.0f;

        java.awt.Color awtColor = new java.awt.Color(java.awt.Color
            .HSBtoRGB(hsb[0], hsb[1], hsb[2]));
        // awtColor = awtColor.brighter();
        // awtColor = awtColor.brighter();
        return new RGB(awtColor.getRed(), awtColor.getGreen(), awtColor.getBlue());
    }

    public static Color createNewSaturatedColor(Color c, float saturation)
    {
        RGB newRGB = saturate(c.getRGB(), saturation);
        return new Color(Display.getCurrent(), newRGB);
    }
}
