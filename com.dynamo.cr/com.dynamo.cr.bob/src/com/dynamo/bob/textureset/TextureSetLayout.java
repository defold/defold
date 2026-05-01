// Copyright 2020-2026 The Defold Foundation
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

import com.dynamo.bob.CompileExceptionError;

/**
 * Atlas layout algorithm(s)
 * @author chmu
 *
 */
public class TextureSetLayout {

    /**
     * Maximum allowed atlas dimension in pixels.
     * This limit ensures compatibility with most graphics hardware and prevents memory issues.
     */
    public static final int MAX_ATLAS_DIMENSION = 16384;

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

    public static class Pointi
    {
        public int x;
        public int y;
        public Pointi() {};
        public Pointi(int x, int y) {
            this.x = x;
            this.y = y;
        };
        public Pointi(Point p) {
            this.x = (int)p.x;
            this.y = (int)p.y;
        };
    }

    public static class Sizei
    {
        public int width;
        public int height;
        public Sizei(int width, int height) {
            this.width = width;
            this.height = height;
        };
    }

    public static class Rectanglei
    {
        public int x;
        public int y;
        public int width;
        public int height;

        public Rectanglei(int x, int y, int width, int height) {
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
        };

        public Rectanglei(Rectangle rect) {
            this.x = (int)rect.x;
            this.y = (int)rect.y;
            this.width = (int)rect.width;
            this.height = (int)rect.height;
        };

        public Point getCenter() {
            return new Point(x + width * 0.5f, y + height * 0.5f);
        }

        public void addBorder(int border) {
            x -= border;
            y -= border;
            width += border * 2;
            height += border * 2;
        }

        public int getX() { return x; }
        public int getY() { return y; }
        public int getWidth() { return width; }
        public int getHeight() { return height; }
    }

    // TODO: Rename this class?
    public static class Rect {
        private String id;
        private int index; // for easier keeping the original order
        private int page;
        private Point pivot; // in image space (texels)
        // Full rect.
        // The final placement in the texture.
        // May lay outside of the texture image.
        private Rectanglei rect;
        private boolean rotated; // True if rotated 90 deg (CW)

        // Texel coordinates within the original image.
        // Origin is top left corner of original image.
        // May be empty, then the rectangle will be used.
        private List<Pointi>     vertices;

        // List of indices (3-tuples) that make up the triangle list
        private List<Integer>    indices;

        public Rect(String id, int index, int page, int x, int y, int width, int height, boolean rotated) {
            this.id = id;
            this.index = index;
            this.page = page;
            this.rotated = rotated;
            this.pivot = new Point(width/2.0f, height/2.0f);
            this.rect = new Rectanglei(x, y, width, height);
            this.vertices = new ArrayList<>();
            this.indices = new ArrayList<>();
        }

        public Rect(String id, int index, int x, int y, int width, int height) {
            this(id, index, -1, x, y, width, height, false);
        }

        public Rect(String id, int index, int width, int height) {
            this(id, index, -1, -1, -1, width, height, false);
        }

        public Rect(Rect other) {
            this(other.id, other.index, other.page, other.rect.x, other.rect.y, other.rect.width, other.rect.height, other.rotated);
            this.vertices = new ArrayList<>(other.vertices);
            this.indices = new ArrayList<>(other.indices);
        }

        public String getId() {
            return id;
        }
        public void setId(String id) {
            this.id = id;
        }
        public int getIndex() {
            return index;
        }
        public void setIndex(int index) {
            this.index = index;
        }
        public int getPage() {
            return page;
        }
        public void setPage(int page) {
            this.page = page;
        }
        public boolean getRotated() {
            return rotated;
        }
        public void setRotated(boolean rotated) {
            this.rotated = rotated;
        }
        public int getX() {
            return rect.x;
        }
        public void setX(int x) {
            rect.x = x;
        }
        public int getY() {
            return rect.y;
        }
        public void setY(int y) {
            rect.y = y;
        }
        public int getWidth() {
            return rect.width;
        }
        public void setWidth(int width) {
            rect.width = width;
        }
        public int getHeight() {
            return rect.height;
        }
        public void setHeight(int height) {
            rect.height = height;
        }
        public int getArea() {
            return rect.width * rect.height;
        }
        public Point getCenter() {
            return rect.getCenter();
        }
        public Point getPivot() {
            return pivot;
        }
        public void setPivot(Point pivot) {
            this.pivot = pivot;
        }
        public List<Pointi> getVertices() {
            return vertices;
        }
        public void addVertex(Pointi vertex) {
            vertices.add(vertex);
        }
        public List<Integer> getIndices() {
            return indices;
        }
        public void setIndices(List<Integer> indices) {
            this.indices = new ArrayList<>(indices);
        }

        public void addBorder(int border) {
            rect.addBorder(border);
        }

        @Override
        public String toString() {
            return String.format("Rect: x/y: %d, %d  w/h: %d, %d  p: %f, %f  r: %d  id: %s", rect.x, rect.y, rect.width, rect.height, pivot.x, pivot.y, rotated?1:0, id);
        }
    }

    // Public api for extensions!
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

    public static List<Layout> packedLayout(int margin, List<Rect> rectangles, boolean rotate, float maxPageSizeW, float maxPageSizeH) throws CompileExceptionError {
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

    public static Layout gridLayout(int margin, List<Rect> rectangles, Grid gridSize ) throws CompileExceptionError {

        // We assume here that all images have the same size,
        // since they will be "packed" into a uniformly sized grid.
        int cellWidth  = rectangles.get(0).rect.width;
        int cellHeight = rectangles.get(0).rect.height;

        // Scale layout area so it's a power of two
        int inputWidth  = gridSize.columns * cellWidth + margin*2;
        int inputHeight = gridSize.rows * cellHeight + margin*2;
        int layoutWidth = 1 << getExponentNextOrMatchingPowerOfTwo(inputWidth);
        int layoutHeight = 1 << getExponentNextOrMatchingPowerOfTwo(inputHeight);
        
        // Validate atlas size limits
        if (layoutWidth > MAX_ATLAS_DIMENSION || layoutHeight > MAX_ATLAS_DIMENSION) {
            throw new CompileExceptionError(String.format(
                "Atlas grid layout size (%dx%d) exceeds maximum allowed dimensions (%dx%d). " +
                "Consider reducing image sizes, using multiple atlases, or using a multi-page atlas.",
                layoutWidth, layoutHeight, MAX_ATLAS_DIMENSION, MAX_ATLAS_DIMENSION, gridSize.columns, gridSize.rows));
        }

        int x = margin;
        int y = margin;

        // Loop through all images and put them right after each other
        Layout layout = new Layout(layoutWidth, layoutHeight, rectangles);
        for (Rect r : layout.getRectangles()) {

            r.setX(x);
            r.setY(y);

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
    public static List<Layout> createMaxRectsLayout(int margin, List<Rect> rectangles, boolean rotate, float maxPageSizeW, float maxPageSizeH) throws CompileExceptionError {
        // Sort by area first, then longest side
        Collections.sort(rectangles, new Comparator<Rect>() {
            @Override
            public int compare(Rect o1, Rect o2) {
                int a1 = o1.getArea();
                int a2 = o2.getArea();
                if (a1 != a2) {
                    return a2 - a1;
                }
                int n1 = Math.max(o1.rect.width, o1.rect.height);
                int n2 = Math.max(o2.rect.width, o2.rect.height);
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
                area += rect.getArea();
                maxLengthScale = Math.max(maxLengthScale, rect.rect.width);
                maxLengthScale = Math.max(maxLengthScale, rect.rect.height);
            }
            maxLengthScale += margin * 2;
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

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Public api for extensions!

    public static class Point
    {
        public float x;
        public float y;
        public Point(float x, float y) {
            this.x = x;
            this.y = y;
        };
        public Point(Point rhs) {
            this.x = rhs.x;
            this.y = rhs.y;
        };

        public float getX()                 { return x; }
        public void setX(float x)           { this.x = x; }
        public float getY()                 { return y; }
        public void setY(float y)           { this.y = y; }
    }

    public static class Size
    {
        public float width;
        public float height;
        public Size(float width, float height) {
            this.width = width;
            this.height = height;
        };

        public float getWidth()                 { return width; }
        public void setWidth(float width)       { this.width = width; }
        public float getHeight()                { return height; }
        public void setHeight(float height)     { this.height = height; }
    }

    public static class Rectangle
    {
        public float x;
        public float y;
        public float width;
        public float height;
        public Rectangle(float x, float y, float width, float height) {
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
        };

        public float getX()                 { return x; }
        public void setX(float x)           { this.x = x; }
        public float getY()                 { return y; }
        public void setY(float y)           { this.y = y; }
        public float getWidth()             { return width; }
        public void setWidth(float width)   { this.width = width; }
        public float getHeight()            { return height; }
        public void setHeight(float height) { this.height = height; }
    }

    public static class SourceImage
    {
        public String       name;           // Name of this image (no path, no suffix)
        public Point        pivot;          // Image coord of pivot point
                                            // * May lay outside of image
                                            // * Not rotated
        public Rectangle    rect;           // The final placement of the rectangle.
                                            // * May overlap other images
                                            // * May be extending beyond the texture.
                                            // * May be rotated
        public boolean      rotated;        // True if it has been rotated 90 deg ccw

        // Texel coordinates within the original image.
        // Origin is top left corner of original image.
        // May be empty, then the rectangle will be used.
        public List<Point>     vertices;

        // List of indices (3-tuples) that make up the triangle list
        public List<Integer>   indices;

        /////////////////////////////////////////////////////////////
        public String getName() {
            return name;
        }
        public void setName(String name) {
            this.name = name;
        }
        public Point getPivot() {
            return pivot;
        }
        public void setPivot(Point pivot) {
            this.pivot = pivot;
        }
        public Rectangle getRect() {
            return rect;
        }
        public void setRect(Rectangle rect) {
            this.rect = rect;
        }
        public boolean getRotated() {
            return rotated;
        }
        public void setRotated(boolean rotated) {
            this.rotated = rotated;
        }

        public List<Point> getVertices() {
            return vertices;
        }
        public void setVertices(List<Point> vertices) {
            this.vertices = vertices;
        }
        public List<Integer> getIndices() {
            return indices;
        }
        public void setIndices(List<Integer> indices) {
            this.indices = indices;
        }

        public void debugPrint() {
            System.out.printf("SourceImage {\n");
            System.out.printf("    name: %s\n", name);
            System.out.printf("    rotated: %s\n", rotated?"true":"false");
            //System.out.printf("    originalSize: %f, %f\n", originalSize.width, originalSize.height);
            System.out.printf("    pivot: %f, %f\n", pivot.x, pivot.y);
            System.out.printf("    rect: %f, %f, %f, %f\n", rect.x, rect.y, rect.width, rect.height);
            System.out.printf("    vertices:  {\n");
            for (Point p : vertices)
            {
                System.out.printf("        %f, %f\n", p.x, p.y);
            }
            System.out.printf("    }\n");
            System.out.printf("    indices:  {\n");
            for (int i : indices)
            {
                System.out.printf("        %d", i);
            }
            System.out.printf("\n");
            System.out.printf("    }\n");
            System.out.printf("}\n");
        }
    }

    // Public api for extensions!
    public static class Page {
        public int                  index; // index in the list of pages
        public String               name;  // name of texture
        public Size                 size;  // size of the texture
        public List<SourceImage>    images;
    }


    // Public api for extensions!
    public static List<Layout> createTextureSet(List<Page> pages) {
        List<Layout> layouts = new ArrayList<>();
        int imageCount = 0;
        for (Page page : pages) {
            Layout layout = new Layout((int)page.size.width, (int)page.size.height, new ArrayList<>());

            for (SourceImage image : page.images) {
                // image.rect is the actual rect within the texture, but we want the "original" size/placement
                int width = (int)image.rect.width;
                int height = (int)image.rect.height;
                Rect rect = new Rect(image.name, imageCount++, (int)image.rect.x, (int)image.rect.y, width, height);
                rect.setPage(page.index);
                rect.setRotated(image.rotated);

                Point p;
                if (image.pivot != null)
                    p = new Point(image.pivot);
                else
                    p = new Point(width/2.0f, height/2.0f);
                rect.setPivot(p);

                for (Point vertex : image.vertices) {
                    rect.addVertex(new Pointi((int)vertex.x, (int)vertex.y));
                }

                rect.indices = new ArrayList<Integer>(image.indices);

                layout.rectangles.add(rect);
            }
            layouts.add(layout);
        }

        return layouts;
    }

    // end Public api!
    /////////////////////////////////////////////////////////////////////////////////////////////////////////

}
