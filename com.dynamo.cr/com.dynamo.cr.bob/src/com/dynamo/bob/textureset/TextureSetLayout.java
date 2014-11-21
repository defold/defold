package com.dynamo.bob.textureset;

import java.util.ArrayList;
import java.util.List;

/**
 * Atlas layout algorithm(s)
 * @author chmu
 *
 */
public class TextureSetLayout {
    public static class Rect {
        public Object id;
        public int x, y, width, height;
        public boolean rotated;

        public Rect(Object id, int x, int y, int width, int height) {
            this.id = id;
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
            this.rotated = false;
        }

        public Rect(Object id, int width, int height) {
            this.id = id;
            this.x = -1;
            this.y = -1;
            this.width = width;
            this.height = height;
            this.rotated = false;
        }

        public Rect(Rect other) {
            this.id = other.id;
            this.x = other.x;
            this.y = other.y;
            this.width = other.width;
            this.height = other.height;
            this.rotated = other.rotated;
        }

        public int area() {
            return width * height;
        }

        @Override
        public String toString() {
            return String.format("[%d, %d, %d, %d]", x, y, width, height);
        }
    }

    public static class Layout {
        private final List<Rect> rectangles;
        private final int width;
        private final int height;
        public Layout(int width, int height, List<Rect> rectangles) {
            this.width = width;
            this.height = height;
            this.rectangles = rectangles;
        }

        public List<Rect> getRectangles() {
            return rectangles;
        }
        public int getWidth() {
            return width;
        }
        public int getHeight() {
            return height;
        }

    }

    public static Layout layout(int margin, List<Rect> rectangles) {
        if (rectangles.size() == 0) {
            return new Layout(1, 1, new ArrayList<TextureSetLayout.Rect>());
        }
        return createMaxRectsLayout(margin, rectangles);
    }

    public static Layout createMaxRectsLayout(int margin, List<Rect> rectangles) {
        int defaultMaxPageSize = 1024;
        final int defaultMinPageSize = 16;

        MaxRectsLayoutStrategy.Settings settings = new MaxRectsLayoutStrategy.Settings();
        settings.maxPageHeight = defaultMaxPageSize;
        settings.maxPageWidth = defaultMaxPageSize;
        settings.minPageHeight = defaultMinPageSize;
        settings.minPageWidth = defaultMinPageSize;
        settings.paddingX = margin;
        settings.paddingY = margin;
        settings.rotation = true;
        settings.fast = false;
        settings.square = false;

        // Ensure the longest length found in all of the images will fit within one page, irrespective of orientation.
        int maxLengthScale = 0;
        for (Rect r : rectangles) {
            maxLengthScale = Math.max(maxLengthScale, r.width + 2 * margin);
            maxLengthScale = Math.max(maxLengthScale, r.height + 2 * margin);
        }
        settings.maxPageHeight = Math.max(settings.maxPageHeight, maxLengthScale);
        settings.maxPageWidth = Math.max(settings.maxPageWidth, maxLengthScale);

        MaxRectsLayoutStrategy strategy = new MaxRectsLayoutStrategy(settings);
        List<Layout> layouts = strategy.createLayout(rectangles);

        while (1 < layouts.size()) {
            settings.maxPageHeight *= 2;
            settings.maxPageWidth *= 2;
            layouts = strategy.createLayout(rectangles);
        }

        return layouts.get(0);
    }
}
