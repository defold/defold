package com.dynamo.cr.tileeditor.atlas;

import java.util.ArrayList;
import java.util.List;

/**
 * Atlas layout algorithm(s)
 * @author chmu
 *
 */
public class AtlasLayout {
    public static class Rect {
        public Object id;
        public int x, y, width, height;

        public Rect(Object id, int x, int y, int width, int height) {
            this.id = id;
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
        }

        public Rect(Object id, int width, int height) {
            this.id = id;
            this.x = -1;
            this.y = -1;
            this.width = width;
            this.height = height;
        }

        public int area() {
            return width * height;
        }

        @Override
        public String toString() {
            return String.format("[%d, %d, %d, %d]", x, y, width, height);
        }
    }

    public static enum LayoutType {
        BASIC
    }

    public static class Layout {
        public final List<Rect> rectangles;
        public final int width;
        public final int height;
        public Layout(int width, int height, List<Rect> rectangles) {
            this.width = width;
            this.height = height;
            this.rectangles = rectangles;
        }
    }

    private static List<Rect> doLayout(int width, int height, int margin, List<Rect> rectangles) {
        List<Rect> result = new ArrayList<AtlasLayout.Rect>(rectangles.size());

        int x = 0, y = 0;
        int rowHeight = 0;

        for (Rect rect : rectangles) {
            if (width - x < rect.width) {
                x = 0;
                y += rowHeight + margin;
            }

            if (width - x < rect.width) {
                return null;
            }

            if (height - y < rect.height) {
                return null;
            }

            result.add(new Rect(rect.id, x, y, rect.width, rect.height));
            x += rect.width + margin;
            rowHeight = Math.max(rowHeight, rect.height);
        }


        return result;
    }

    /**
     * Layout rectangles according to algorithm and margin.
     * @note Margin is only added between internal rectangles and not to the border. The
     * margin specifies the total space between rectangles and not margin for individual rectangles.
     * @param layout layout method
     * @param margin total margin between internal tiles
     * @param rectangles rectangles to layout
     * @return {@link Layout} result
     */
    public static Layout layout(LayoutType layout, int margin, List<Rect> rectangles) {
        int totalArea = 0;
        for (Rect rect : rectangles) {
            totalArea += rect.area();
        }

        int tmp = (int) Math.sqrt(totalArea);
        int width = closestPowerTwoDown(tmp);
        int height = width;

        List<Rect> result = null;
        while (true) {
            result = doLayout(width, height, margin, rectangles);
            if (result == null) {
                if (width <= height) {
                    width *= 2;
                } else {
                    height *= 2;
                }

            } else {
                break;
            }
        }
        return new Layout(width, height, result);
    }

    public static int closestPowerTwoDown(int i) {
        int nextPow2 = 1;
        while (nextPow2 <= i) {
            nextPow2 *= 2;
        }
        return Math.max(1, nextPow2 / 2);
    }

}
