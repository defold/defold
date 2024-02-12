// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.textureset;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Arrays;

/**
 * Atlas layout algorithm(s)
 * @author chmu
 *
 */
public class TextureSetLayout {

    public static class Grid {
        public int columns;
        public int rows;

        public Grid( int columns, int rows ) {
            this.columns = columns;
            this.rows    = rows;
        }

        @Override
        public String toString() {
            return String.format("%d columns, %d rows", columns, rows);
        }
    }

    public static class Rect {
        public String id;
        public int index; // for easier keeping the original order
        public int x, y, width, height;
        public int page;
        public boolean rotated;

        public Rect(String id, int index, int x, int y, int width, int height) {
            this.id = id;
            this.index = index;
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
            this.rotated = false;
        }

        public Rect(String id, int index, int width, int height) {
            this.id = id;
            this.index = index;
            this.x = -1;
            this.y = -1;
            this.width = width;
            this.height = height;
            this.rotated = false;
        }

        public Rect(Rect other) {
            this.id = other.id;
            this.index = other.index;
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
            return String.format("Rect: x/y: %d, %d  w/h: %d, %d  r: %d  id: %s", x, y, width, height, rotated?1:0, id);
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

        @Override
        public String toString() {
            String s = "Layout:\n";
            s += String.format("  width: %d:\n", width);
            s += String.format("  height: %d:\n", height);
            for (Rect r : rectangles) {
                s += String.format("  %s\n", r.toString());
            }
            s += "\n";
            return s;
        }
    }

    public static List<Layout> packedLayout(int margin, List<Rect> rectangles, boolean rotate, float maxPageSizeW, float maxPageSizeH) {
        if (rectangles.size() == 0) {
            return Arrays.asList(new Layout(1, 1, new ArrayList<TextureSetLayout.Rect>()));
        }

        return createMaxRectsLayout(margin, rectangles, rotate, maxPageSizeW, maxPageSizeH);
    }

    private static int getExponentNextOrMatchingPowerOfTwo(int value) {
        int exponent = 0;
        while (value > (1<<exponent)) {
            ++exponent;
        }
        return exponent;
    }

    public static Layout gridLayout(int margin, List<Rect> rectangles, Grid gridSize ) {

        // We assume here that all images have the same size,
        // since they will be "packed" into a uniformly sized grid.
        int cellWidth  = rectangles.get(0).width;
        int cellHeight = rectangles.get(0).height;

        // Scale layout area so it's a power of two
        int inputWidth  = gridSize.columns * cellWidth + margin*2;
        int inputHeight = gridSize.rows * cellHeight + margin*2;
        int layoutWidth = 1 << getExponentNextOrMatchingPowerOfTwo(inputWidth);
        int layoutHeight = 1 << getExponentNextOrMatchingPowerOfTwo(inputHeight);

        int x = margin;
        int y = margin;

        // Loop through all images and put them right after each other
        Layout layout = new Layout(layoutWidth, layoutHeight, rectangles);
        for (Rect r : layout.getRectangles()) {

            r.x = x;
            r.y = y;

             if (x + cellWidth >= inputWidth - margin) {
                 x = margin;
                 y += cellHeight;
             } else {
                 x += cellWidth;
             }

        }

        return layout;
    }

    /**
     * @param margin
     * @param rectangles
     * @param rotate
     * @return
     */
    public static List<Layout> createMaxRectsLayout(int margin, List<Rect> rectangles, boolean rotate, float maxPageSizeW, float maxPageSizeH) {
        // Sort by area first, then longest side
        Collections.sort(rectangles, new Comparator<Rect>() {
            @Override
            public int compare(Rect o1, Rect o2) {
                int a1 = o1.width * o1.height;
                int a2 = o2.width * o2.height;
                if (a1 != a2) {
                    return a2 - a1;
                }
                int n1 = o1.width > o1.height ? o1.width : o1.height;
                int n2 = o2.width > o2.height ? o2.width : o2.height;
                return n2 - n1;
            }
        });

        boolean useMaxPageSize       = maxPageSizeW > 0 && maxPageSizeH > 0;
        final int defaultMinPageSize = 16;

        if (useMaxPageSize) {
            MaxRectsLayoutStrategy.Settings settings = new MaxRectsLayoutStrategy.Settings();
            settings.maxPageHeight = (int) maxPageSizeH;
            settings.maxPageWidth  = (int) maxPageSizeW;
            settings.minPageHeight = defaultMinPageSize;
            settings.minPageWidth  = defaultMinPageSize;
            settings.paddingX      = margin;
            settings.paddingY      = margin;
            settings.rotation      = rotate;
            settings.square        = false;

            MaxRectsLayoutStrategy strategy = new MaxRectsLayoutStrategy(settings);
            List<Layout> layouts = strategy.createLayout(rectangles);

            int maxWidth  = -1;
            int maxHeight = -1;
            int minWidth  = Integer.MAX_VALUE;
            int minHeight = Integer.MAX_VALUE;

            for (Layout l : layouts)
            {
                maxWidth  = Math.max(maxWidth, l.getWidth());
                maxHeight = Math.max(maxHeight, l.getHeight());
                minWidth  = Math.min(minWidth, l.getWidth());
                minHeight = Math.min(minHeight, l.getHeight());
            }

            // The layout strategy might not guarantee that all layouts have the same dimensions,
            // so we need to make sure this is the case
            if (maxWidth != minWidth || maxHeight != minHeight)
            {
                for (int i=0; i < layouts.size(); i++)
                {
                    if (layouts.get(i).getWidth() != maxWidth || layouts.get(i).getHeight() != maxHeight)
                    {
                        layouts.set(i, new Layout(maxWidth, maxHeight, layouts.get(i).getRectangles()));
                    }
                }
            }

            return layouts;
        } else {
            // Calculate total area of rectangles and the max length of a rectangle
            int maxLengthScale = 0;
            int area = 0;
            for (Rect rect : rectangles) {
                area += rect.area();
                maxLengthScale = Math.max(maxLengthScale, rect.width + margin);
                maxLengthScale = Math.max(maxLengthScale, rect.height + margin);
            }

            // Ensure the longest length found in all of the images will fit within one page, irrespective of orientation.
            final int defaultMaxPageSize = 1 << getExponentNextOrMatchingPowerOfTwo(Math.max((int)Math.sqrt(area), maxLengthScale));
            MaxRectsLayoutStrategy.Settings settings = new MaxRectsLayoutStrategy.Settings();
            settings.maxPageHeight = defaultMaxPageSize;
            settings.maxPageWidth = defaultMaxPageSize;
            settings.minPageHeight = defaultMinPageSize;
            settings.minPageWidth = defaultMinPageSize;
            settings.paddingX = margin;
            settings.paddingY = margin;
            settings.rotation = rotate;
            settings.square = false;

            MaxRectsLayoutStrategy strategy = new MaxRectsLayoutStrategy(settings);
            List<Layout> layouts = strategy.createLayout(rectangles);

            // Repeat layout creation using alternating increase of width and height until
            // all rectangles can be fit into a single layout
            int iterations = 0;
            while (layouts.size() > 1) {
                iterations++;
                settings.minPageWidth = settings.maxPageWidth;
                settings.minPageHeight = settings.maxPageHeight;
                settings.maxPageHeight *= (iterations % 2) == 0 ? 2 : 1;
                settings.maxPageWidth *= (iterations % 2) == 0 ? 1 : 2;
                layouts = strategy.createLayout(rectangles);
            }

            return layouts.subList(0,1);
        }
    }
}
