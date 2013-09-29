package com.dynamo.cr.guieditor.test;

import static org.junit.Assert.assertEquals;

import java.awt.Rectangle;
import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.guieditor.render.IGuiRenderer.TextLine;
import com.dynamo.cr.guieditor.util.TextUtil;
import com.dynamo.cr.guieditor.util.TextUtil.ITextMetric;

public class TextUtilTest {
    private static final int CHAR_SIZE = 10;
    ITextMetric metric;

    @Before
    public void setUp() throws Exception {
        metric = new ITextMetric() {
            @Override
            public Rectangle getVisualBounds(String text) {
                return new Rectangle(0, 0, text.length() * CHAR_SIZE, CHAR_SIZE);
            }

            @Override
            public float getLSB(char c) {
                return 0;
            }
        };
    }

    private List<IGuiRenderer.TextLine> layout(String text, double width, boolean lineBreak) {
        return TextUtil.layout(metric, text, width, lineBreak);
    }

    @Test
    public void testNonBreaking() throws Exception {
        String text = "No breaks in this text";
        double w = 1;
        List<TextLine> lines = layout(text, w, false);
        assertEquals(1, lines.size());
        assertEquals(text, lines.get(0).text);
        assertEquals(text.length() * CHAR_SIZE, lines.get(0).width, 0);
    }

    @Test
    public void testEnoughWidth() throws Exception {
        String text = "No breaks in this text";
        double w = text.length() * CHAR_SIZE;
        List<TextLine> lines = layout(text, w, true);
        assertEquals(1, lines.size());
        assertEquals(text, lines.get(0).text);
        assertEquals(w, lines.get(0).width, 0);
    }

    @Test
    public void testBreakEvery() throws Exception {
        String text = "All breaks in this text";
        double w = 10;
        List<TextLine> lines = layout(text, w, true);
        String[] words = text.split(" ");
        assertEquals(words.length, lines.size());
        for (int i = 0; i < words.length; ++i) {
            assertEquals(words[i], lines.get(i).text);
            assertEquals(words[i].length() * CHAR_SIZE, lines.get(i).width, 0);
        }
    }

    @Test
    public void testMultipleWS() throws Exception {
        String text = "Two     breaks";
        double w = 10;
        List<TextLine> lines = layout(text, w, true);
        String[] words = new String[] { "Two", "breaks" };
        assertEquals(words.length, lines.size());
        for (int i = 0; i < lines.size(); ++i) {
            assertEquals(words[i], lines.get(i).text);
        }
    }

    @Test
    public void testTrailingWS() throws Exception {
        String text = " Two breaks ";
        double w = 10;
        List<TextLine> lines = layout(text, w, true);
        String[] words = new String[] { "Two", "breaks" };
        assertEquals(words.length, lines.size());
        for (int i = 0; i < lines.size(); ++i) {
            assertEquals(words[i], lines.get(i).text);
        }
    }

    @Test
    public void testManualBreaks() throws Exception {
        String text = "Line one\nLine two";
        double w = 100;
        List<TextLine> lines = layout(text, w, true);
        String[] words = new String[] { "Line one", "Line two" };
        assertEquals(words.length, lines.size());
        for (int i = 0; i < lines.size(); ++i) {
            assertEquals(words[i], lines.get(i).text);
        }
    }
}
