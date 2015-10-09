package com.dynamo.cr.guied.util;

import java.util.ArrayList;
import java.util.List;

import org.apache.commons.lang3.StringUtils;

import com.dynamo.cr.guied.ui.TextNodeRenderer.TextLine;
import com.dynamo.cr.sceneed.core.FontRendererHandle;
import com.dynamo.render.proto.Font.FontMap;

public class TextUtil {

    public static class TextMetric {

        public FontRendererHandle textHandler;

        public TextMetric(FontRendererHandle textHandler) {
            this.textHandler = textHandler;
        }

        private double getLineTextMetrics(String text, int n) {
            double w = 0.0;
            FontMap.Glyph last = null;

            if (!text.isEmpty()) {

                for (int i = 0; i < text.length() && i < n; i++) {
                    char c = text.charAt(i);
                    FontMap.Glyph g = this.textHandler.getGlyph(c);
                    if (g == null) {
                        continue;
                    }

                    last = g;

                    w += g.getAdvance();
                }

                if (last != null) {
                    double last_end_point = last.getLeftBearing() + last.getWidth();
                    double last_right_bearing = last.getAdvance() - last_end_point;
                    w = w - last_right_bearing;
                }
            }

            return w;
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
            double w = 0.0, last_w = 0.0;

            do {
                cursor.setN(n);
                c = NextBreak(cursor);
                n = cursor.getN();
                if (n > 0) {

                    int trim = 0;
                    if (c != 0)
                        trim = 1;

                    w = metric.getLineTextMetrics(row_start.getText(), n-trim);
                    if (w <= width) {
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

            } while (w <= width && c != 0 && c != '\n');

            if (w > width && last_n == 0) {
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
