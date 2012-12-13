package com.dynamo.cr.tileeditor.atlas;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import java.util.Arrays;
import java.util.List;

import org.junit.Test;

import com.dynamo.cr.tileeditor.atlas.AtlasLayout.Layout;
import com.dynamo.cr.tileeditor.atlas.AtlasLayout.Rect;

public class AtlasLayoutTest {

    @Test
    public void testClosestPowerTwo() {
        assertEquals(64, AtlasLayout.closestPowerTwoDown(127));
        assertEquals(128, AtlasLayout.closestPowerTwoDown(129));

        assertEquals(1, AtlasLayout.closestPowerTwoDown(1));
        assertEquals(2, AtlasLayout.closestPowerTwoDown(2));
        assertEquals(2, AtlasLayout.closestPowerTwoDown(3));
        assertEquals(4, AtlasLayout.closestPowerTwoDown(5));
        assertEquals(4, AtlasLayout.closestPowerTwoDown(6));
        assertEquals(4, AtlasLayout.closestPowerTwoDown(7));
        assertEquals(8, AtlasLayout.closestPowerTwoDown(8));
        assertEquals(8, AtlasLayout.closestPowerTwoDown(9));
        assertEquals(8, AtlasLayout.closestPowerTwoDown(10));
    }

    void assertRect(Layout layout, int i, int x, int y, Object id) {
        Rect r = layout.getRectangles().get(i);
        assertThat(r.x, is(x));
        assertThat(r.y, is(y));
        assertThat(r.id, is(id));
    }

    @Test
    public void testEmpty() {
        List<AtlasLayout.Rect> rectangles
            = Arrays.asList();

        Layout layout = AtlasLayout.layout(AtlasLayout.LayoutType.BASIC, 0, rectangles);
        assertThat(layout.getWidth(), is(1));
        assertThat(layout.getHeight(), is(1));
        assertThat(layout.getRectangles().size(), is(0));
    }

    @Test
    public void testBasic1() {
        List<AtlasLayout.Rect> rectangles
            = Arrays.asList(rect(0, 16, 16),
                            rect(1, 16, 16),
                            rect(2, 16, 16),
                            rect(3, 16, 16));

        Layout layout = AtlasLayout.layout(AtlasLayout.LayoutType.BASIC, 0, rectangles);
        assertThat(layout.getWidth(), is(32));
        assertThat(layout.getHeight(), is(32));
        assertRect(layout, 0, 0, 0, 0);
        assertRect(layout, 1, 16, 0, 1);
        assertRect(layout, 2, 0, 16, 2);
        assertRect(layout, 3, 16, 16, 3);
    }

    @Test
    public void testBasic2() {
        List<AtlasLayout.Rect> rectangles
            = Arrays.asList(rect(0, 16, 16),
                            rect(1, 8, 8),
                            rect(2, 8, 8),
                            rect(3, 8, 8),
                            rect(4, 8, 8),
                            rect(5, 8, 8),
                            rect(6, 8, 8));

        Layout layout = AtlasLayout.layout(AtlasLayout.LayoutType.BASIC, 0, rectangles);
        assertThat(layout.getWidth(), is(32));
        assertThat(layout.getHeight(), is(32));
        assertRect(layout, 0, 0, 0, 0);
        assertRect(layout, 1, 16, 0, 1);
        assertRect(layout, 2, 24, 0, 2);
        assertRect(layout, 3, 0, 16, 3);
        assertRect(layout, 4, 8, 16, 4);
        assertRect(layout, 5, 16, 16, 5);
        assertRect(layout, 6, 24, 16, 6);
    }

    @Test
    public void testBasic3() {
        List<AtlasLayout.Rect> rectangles
            = Arrays.asList(rect(0, 512, 128));

        Layout layout = AtlasLayout.layout(AtlasLayout.LayoutType.BASIC, 0, rectangles);
        assertThat(layout.getWidth(), is(512));
        assertThat(layout.getHeight(), is(128));
        assertRect(layout, 0, 0, 0, 0);
    }

    @Test
    public void testBasic4() {
        List<AtlasLayout.Rect> rectangles
            = Arrays.asList(rect(0, 32, 12),
                            rect(1, 16, 2),
                            rect(2, 16, 2));

        Layout layout = AtlasLayout.layout(AtlasLayout.LayoutType.BASIC, 0, rectangles);
        assertThat(layout.getWidth(), is(32));
        assertThat(layout.getHeight(), is(16));
        assertRect(layout, 0, 0, 0, 0);
        assertRect(layout, 1, 0, 12, 1);
        assertRect(layout, 2, 16, 12, 2);
    }

    @Test
    public void testBasicMargin1() {
        List<AtlasLayout.Rect> rectangles
            = Arrays.asList(rect(0, 16, 16),
                            rect(1, 16, 16),
                            rect(2, 16, 16),
                            rect(3, 16, 16));

        Layout layout = AtlasLayout.layout(AtlasLayout.LayoutType.BASIC, 2, rectangles);
        assertThat(layout.getWidth(), is(64));
        assertThat(layout.getHeight(), is(64));
        assertRect(layout, 0, 0, 0, 0);
        assertRect(layout, 1, 16 + 2, 0, 1);
        assertRect(layout, 2, 16 * 2 + 2 * 2, 0, 2);
        assertRect(layout, 3, 0, 16 + 2, 3);
    }

    @Test
    public void testBasicMargin2() {
        List<AtlasLayout.Rect> rectangles
            = Arrays.asList(rect(0, 15, 15),
                            rect(1, 15, 15),
                            rect(2, 15, 15),
                            rect(3, 15, 15));

        Layout layout = AtlasLayout.layout(AtlasLayout.LayoutType.BASIC, 2, rectangles);
        assertThat(layout.getWidth(), is(32));
        assertThat(layout.getHeight(), is(32));
        assertRect(layout, 0, 0, 0, 0);
        assertRect(layout, 1, 15 + 2, 0, 1);
        assertRect(layout, 2, 0, 15 + 2, 2);
        assertRect(layout, 3, 15 + 2, 15 + 2, 3);
    }

    private Rect rect(Object id, int w, int h) {
        return new Rect(id, w, h);
    }

}
