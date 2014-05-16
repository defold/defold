package com.dynamo.cr.guied.util;

import java.awt.Font;
import java.awt.Rectangle;
import java.awt.font.FontRenderContext;
import java.awt.font.GlyphVector;
import java.util.ArrayList;
import java.util.List;

import org.apache.commons.lang3.StringUtils;

import com.dynamo.cr.guied.ui.TextNodeRenderer.TextLine;
import com.jogamp.opengl.util.awt.TextRenderer;

public class TextUtil {
    public static class TextMetric {
        public TextMetric(TextRenderer textRenderer) {
            this.font = textRenderer.getFont();
            this.fontRenderContext = textRenderer.getFontRenderContext();
        }

        public Rectangle getVisualBounds(String text) {
            GlyphVector glyphVector = font.createGlyphVector(this.fontRenderContext, text);
            return glyphVector.getOutline().getBounds();
        }

        public float getLSB(char c) {
            GlyphVector glyphVector = font.createGlyphVector(this.fontRenderContext, "" + c);
            return glyphVector.getGlyphMetrics(0).getLSB();
        }

        private Font font;
        private FontRenderContext fontRenderContext;
    }

    private static double getWidth(TextMetric metric, String text) {
        if (!text.isEmpty()) {
            return metric.getVisualBounds(text).getWidth() + metric.getLSB(text.charAt(0));
        }
        return 0;
    }

    public static List<TextLine> layout(TextMetric metric, String text, double width, boolean lineBreak) {
        text = StringUtils.stripEnd(text, null);

        List<TextLine> textLines = new ArrayList<TextLine>(128);
        if (!lineBreak) {
            textLines.add(new TextLine(text, getWidth(metric, text)));
            return textLines;
        }
        // pre-break at all new line characters
        String[] lines = text.split("\n");
        for (String line : lines) {
            line = StringUtils.strip(line);
            int length = line.length();
            int i = 0;
            do {
                char c = 0;
                double w = 0.0;
                double lastW = 0.0;
                int j = i;
                int lastFit = 0;
                do {
                    // heuristic is to break whenever a non-ws char, followed by a ws char, overflows the width
                    for (; j < length; ++j) {
                        c = line.charAt(j);
                        if (c == 0 || Character.isWhitespace(c))
                            break;
                    }
                    w = getWidth(metric, line.substring(i, j));
                    if (w <= width) {
                        lastFit = j;
                        lastW = w;
                        for (; j < length; ++j) {
                            c = line.charAt(j);
                            if (c == 0 || !Character.isWhitespace(c))
                                break;
                        }
                    }
                } while (w <= width && j < length);
                if (lastFit == 0) {
                    lastFit = j;
                    lastW = w;
                }
                textLines.add(new TextLine(line.substring(i, lastFit), lastW));
                if (lastFit < length) {
                    line = StringUtils.stripStart(line.substring(lastFit), null);
                    length = line.length();
                    i = 0;
                } else {
                    i = length;
                }
            } while (i < length);
        }
        return textLines;
    }
}
