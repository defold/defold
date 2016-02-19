package com.dynamo.cr.guied.util;

import java.util.ArrayList;
import java.util.List;

import org.apache.commons.lang3.StringUtils;

import com.dynamo.cr.guied.ui.TextNodeRenderer.TextLine;
import com.dynamo.cr.sceneed.core.FontRendererHandle;
import com.dynamo.render.proto.Font.FontMap;

public class TextUtil {

    public static class TextLineSize {
        public double minX;
        public double maxX;
        public TextLineSize(double minx, double maxx) {
            this.minX = minx;
            this.maxX = maxx;
        }

        public double getWidth() {
            return maxX - minX;
        }
    }

    public static class TextMetric {

        public FontRendererHandle textHandler;
        private double tracking;

        public TextMetric(FontRendererHandle textHandler, double tracking) {
            this.textHandler = textHandler;
            this.tracking = tracking;
        }

        private TextLineSize getLineTextMetrics(String text, int n) {
            FontMap.Glyph last = null;

            double minx = 0;
            double maxx = 0;

            if (!text.isEmpty()) {

                double w = 0.0;
                for (int i = 0; i < text.length() && i < n; i++) {
                    if( i > 0 )
                    {
                        w += this.tracking;
                    }

                    char c = text.charAt(i);
                    FontMap.Glyph g = this.textHandler.getGlyph(c);
                    if (g == null) {
                        continue;
                    }

                    last = g;

                    w += g.getAdvance();

                    minx = Math.min(w, minx);
                    maxx = Math.max(w, maxx);
                }

                if (last != null) {
                    double last_end_point = last.getLeftBearing() + last.getWidth();
                    double last_right_bearing = last.getAdvance() - last_end_point;
                    w = w - last_right_bearing;

                    minx = Math.min(w, minx);
                    maxx = Math.max(w, maxx);
                }
            }
            return new TextLineSize(minx, maxx);
        }

    }

    private static class TextCursor {
        private String text;
        private int cur;
        private int n;
        TextCursor(String text) {
            this.text = text;
            this.cur = 0;
            this.n = 0;
        }
        TextCursor(TextCursor cursor) {
            this.text = cursor.getText();
            this.cur = 0;
            this.n = 0;
        }
        public void setN(int n) { this.n = n; }
        public int getN() { return this.n; }
        public String getText() { return this.text.substring(this.cur); }
        public char nextChar() {
            if (this.text.length() < this.cur + 1) {
                return 0;
            }
            return this.text.charAt(this.cur++);
        }
    }

    private static char SkipWS(TextCursor cursor) {
        char c = 0;
        int n = cursor.getN();
        do {
            c = cursor.nextChar();
            if (c != 0)
                n += 1;
        } while (c != 0 && c == ' ');

        cursor.setN(n);
        return c;
    }

    private static boolean IsBreaking(char c) {
        return c == ' ' || c == '\n';
    }

    private static char NextBreak(TextCursor cursor) {
        char c = 0;
        int n = cursor.getN();
        do {
            c = cursor.nextChar();
            if (c != 0)
                n += 1;
        } while (c != 0 && !IsBreaking(c));

        cursor.setN(n);
        return c;
    }

    //
    // Java implementation of the C version from font_renderer_private.h
    // Want to have close to 1:1 layout of text nodes in editor <-> engine.
    //
    public static List<TextLine> layout(TextMetric metric, String text, double width, boolean lineBreak) {
        text = StringUtils.stripEnd(text, null);
        List<TextLine> textLines = new ArrayList<TextLine>(128);

        if (!lineBreak) {
            textLines.add(new TextLine(text, metric.getLineTextMetrics(text, text.length())));
            return textLines;
        }

        char c;
        TextCursor cursor = new TextCursor(text);
        do {

            int n = 0, last_n = 0;
            TextCursor row_start = new TextCursor(cursor);
            TextCursor last_cursor = new TextCursor(cursor);

            TextLineSize w = new TextLineSize(0, 0);
            TextLineSize last_w = new TextLineSize(0, 0);

            do {
                cursor.setN(n);
                c = NextBreak(cursor);
                n = cursor.getN();
                if (n > 0) {

                    int trim = 0;
                    if (c != 0)
                        trim = 1;

                    w = metric.getLineTextMetrics(row_start.getText(), n-trim);
                    if (Math.abs(w.getWidth()) <= width) {
                        last_n = n-trim;
                        last_w = w;
                        last_cursor = new TextCursor(cursor);
                        if (c != '\n') {
                            cursor.setN(n);
                            c = SkipWS(cursor);
                            n = cursor.getN();
                        }
                    } else if (last_n != 0) {
                        // rewind if we have more to scan
                        cursor = new TextCursor(last_cursor);
                        c = last_cursor.nextChar();
                    }

                }

            } while (Math.abs(w.getWidth()) <= width && c != 0 && c != '\n');

            if (Math.abs(w.getWidth()) > width && last_n == 0) {
                int trim = 0;
                if (c != 0)
                    trim = 1;
                last_n = n - trim;
                last_w = w;
            }

            if (c != 0 || last_n > 0) {
                textLines.add(new TextLine(row_start.getText().substring(0, last_n), last_w));
            }

        } while (c != 0);

        return textLines;
    }
}
