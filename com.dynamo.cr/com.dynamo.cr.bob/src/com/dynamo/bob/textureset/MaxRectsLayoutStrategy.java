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
import java.util.HashMap;
import java.util.List;

import static com.dynamo.bob.textureset.TextureSetLayout.MAX_ATLAS_DIMENSION;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.textureset.TextureSetLayout.Layout;
import com.dynamo.bob.textureset.TextureSetLayout.Rect;

/**
 * MaxRectsLayoutStrategy - derived from libgdx implementation:
 * https://github.com/libgdx/libgdx/blob/master/extensions/gdx-tools/src/com/badlogic/gdx/tools/texturepacker/MaxRectsPacker.java
 * @author peterhodges
 *
 */
public class MaxRectsLayoutStrategy implements TextureSetLayoutStrategy {

    public static class Settings {
        public int maxPageWidth;
        public int maxPageHeight;
        public int minPageWidth;
        public int minPageHeight;
        public int paddingX;
        public int paddingY;
        public boolean rotation;
        public boolean square;
    }

    private Settings settings;
    private MaxRects maxRects = new MaxRects();
    private FreeRectChoiceHeuristic[] methods = FreeRectChoiceHeuristic.values();

    public MaxRectsLayoutStrategy(Settings settings) {
        this.settings = settings;
    }

    @Override
    public List<Layout> createLayout(List<Rect> srcRects) throws CompileExceptionError {
        ArrayList<RectNode> srcNodes = new ArrayList<RectNode>(srcRects.size());
        for(Rect r : srcRects) {
            RectNode n = new RectNode(r);
            n.rect.setWidth(n.rect.getWidth() + settings.paddingX);
            n.rect.setHeight(n.rect.getHeight() + settings.paddingY);
            srcNodes.add(n);
        }

        ArrayList<Page> pages = new ArrayList<Page>();
        while (!srcNodes.isEmpty()) {
            Page result = packPage(srcNodes);
            pages.add(result);
            srcNodes = result.remainingRects;
        }

        // Repackage into layouts.
        ArrayList<Layout> result = new ArrayList<Layout>(pages.size());
        for(Page page : pages) {
            ArrayList<Rect> rects = new ArrayList<Rect>(page.outputRects.size());
            for(RectNode node : page.outputRects) {
                Rect finalRect = new Rect(node.rect.getId(), node.rect.getIndex(), node.rect.getPage(),
                                           node.rect.getX() + settings.paddingX, node.rect.getY() + settings.paddingY,
                                           node.rect.getWidth() - settings.paddingX, node.rect.getHeight() - settings.paddingY,
                                           node.rect.getRotated());
                rects.add(finalRect);
            }
            int layoutWidth = 1 << getExponentNextOrMatchingPowerOfTwo(page.width);
            int layoutHeight = 1 << getExponentNextOrMatchingPowerOfTwo(page.height);

            // Validate atlas size limits
            if (layoutWidth > MAX_ATLAS_DIMENSION || layoutHeight > MAX_ATLAS_DIMENSION) {
                throw new CompileExceptionError(String.format(
                    "Atlas layout size (%dx%d) exceeds maximum allowed dimensions (%dx%d). " +
                    "Consider reducing image sizes, using multiple atlases, or using a multi-page atlas.",
                    layoutWidth, layoutHeight, MAX_ATLAS_DIMENSION, MAX_ATLAS_DIMENSION));
            }
            
            Layout layout = new Layout(layoutWidth, layoutHeight, rects);
            result.add(layout);
        }

        return result;
    }

    private Page packPage(ArrayList<RectNode> inputRects) {
        // Find min size.
        int minWidth = Integer.MAX_VALUE;
        int minHeight = Integer.MAX_VALUE;
        int maxRectWidth = 0;
        int maxRectHeight = 0;
        long totalRectArea = 0L;
        for (int i = 0, nn = inputRects.size(); i < nn; i++) {
            Rect rect = inputRects.get(i).rect;
            minWidth = Math.min(minWidth, rect.getWidth());
            minHeight = Math.min(minHeight, rect.getHeight());
            maxRectWidth = Math.max(maxRectWidth, rect.getWidth());
            maxRectHeight = Math.max(maxRectHeight, rect.getHeight());
            totalRectArea += (long)rect.getWidth() * (long)rect.getHeight();
            if (settings.rotation) {
                if ((rect.getWidth() > settings.maxPageWidth - settings.paddingX || rect.getHeight() > settings.maxPageHeight - settings.paddingY)
                    && (rect.getWidth() > settings.maxPageHeight - settings.paddingY || rect.getHeight() > settings.maxPageWidth - settings.paddingX)) {
                    throw new RuntimeException("Image does not fit with max page size " + settings.maxPageWidth + "x" + settings.maxPageHeight
                        + " and padding " + settings.paddingX + "," + settings.paddingY + ": " + rect);
                }
            } else {
                if (rect.getWidth() > settings.maxPageWidth - settings.paddingX) {
                    throw new RuntimeException("Image does not fit with max page width " + settings.maxPageWidth + " and paddingX "
                        + settings.paddingX + ": " + rect);
                }
                if (rect.getHeight() > settings.maxPageHeight - settings.paddingY) {
                    throw new RuntimeException("Image does not fit in max page height " + settings.maxPageHeight + " and paddingY "
                        + settings.paddingY + ": " + rect);
                }
            }
        }
        minWidth = Math.max(minWidth + settings.paddingX, settings.minPageWidth);
        minHeight = Math.max(minHeight + settings.paddingY, settings.minPageHeight);
        RectStats rectStats = new RectStats(maxRectWidth, maxRectHeight, totalRectArea);

        // Find the minimal page size that fits all rects.
        Page bestResult = null;
        if (settings.square) {
            int minSize = Math.max(minWidth, minHeight);
            int maxSize = Math.min(settings.maxPageWidth, settings.maxPageHeight);
            BinarySearch sizeSearch = new BinarySearch(minSize, maxSize);
            int size = sizeSearch.reset();
            while (size != -1) {
                Page result = packAtSize(true, size - settings.paddingX, size - settings.paddingY, inputRects, rectStats, settings.rotation);
                bestResult = getBest(bestResult, result);
                size = sizeSearch.next(result == null);
            }

            // Rects don't fit on one page. Fill a whole page and return.
            if (bestResult == null) {
                bestResult = packAtSize(false, maxSize - settings.paddingX, maxSize - settings.paddingY, inputRects, rectStats, settings.rotation);
            }

             bestResult.width = Math.max(bestResult.width, bestResult.height);
             bestResult.height = Math.max(bestResult.width, bestResult.height);
        } else {
            BinarySearch widthSearch = new BinarySearch(minWidth, settings.maxPageWidth);
            BinarySearch heightSearch = new BinarySearch(minHeight, settings.maxPageHeight);
            int width = widthSearch.reset();
            int height = heightSearch.reset();
            while (true) {
                Page bestWidthResult = null;
                while (width != -1) {
                    Page result = packAtSize(true, width - settings.paddingX, height - settings.paddingY, inputRects, rectStats, settings.rotation);
                    bestWidthResult = getBest(bestWidthResult, result);
                    width = widthSearch.next(result == null);
                }
                bestResult = getBest(bestResult, bestWidthResult);
                height = heightSearch.next(bestWidthResult == null);
                if (height == -1) {
                    break;
                }
                width = widthSearch.reset();
            }
            // Rects don't fit on one page. Fill a whole page and return.
            if (bestResult == null) {
                bestResult = packAtSize(false, settings.maxPageWidth - settings.paddingX, settings.maxPageHeight - settings.paddingY, inputRects, rectStats, settings.rotation);
            }
        }
        return bestResult;
    }

    /** @param fully If true, the only results that pack all rects will be considered. If false, all results are considered, not all
     *           rects may be packed.
     **/
    private Page packAtSize(boolean fully, int width, int height, ArrayList<RectNode> inputRects, RectStats rectStats, boolean checkRotation) {
        if (!canFitInPage(width, height, rectStats, fully, checkRotation)) {
            return null;
        }

        Page bestResult = null;
        for (int i = 0, n = methods.length; i < n; i++) {
            maxRects.init(width, height);
            Page result;

            ArrayList<RectNode> remaining = new ArrayList<RectNode>();
            for (int ii = 0, nn = inputRects.size(); ii < nn; ii++) {
                RectNode rect = inputRects.get(ii);
                if (maxRects.insert(rect, methods[i]) == null) {
                    while (ii < nn) {
                        remaining.add(inputRects.get(ii++));
                    }
                }
            }
            result = maxRects.getResult();
            result.remainingRects = remaining;

            if (fully && !result.remainingRects.isEmpty()) {
                continue;
            }
            if (result.outputRects.isEmpty()) {
                continue;
            }
            bestResult = getBest(bestResult, result);
        }
        return bestResult;
    }

    private Page getBest (Page result1, Page result2) {
        if (result1 == null) return result2;
        if (result2 == null) return result1;
        return result1.occupancy > result2.occupancy ? result1 : result2;
    }

    static int getExponentNextOrMatchingPowerOfTwo(int value) {
        int exponent = 0;
        while (value > (1<<exponent)) {
            ++exponent;
        }
        return exponent;
    }

    private boolean canFitInPage(int width, int height, RectStats stats, boolean fully, boolean checkRotation) {
        long availableArea = (long)width * (long)height;
        if (fully && availableArea < stats.totalArea) {
            return false;
        }

        if (!checkRotation) {
            return width >= stats.maxWidth && height >= stats.maxHeight;
        }

        boolean fitsUpright = width >= stats.maxWidth && height >= stats.maxHeight;
        boolean fitsRotated = width >= stats.maxHeight && height >= stats.maxWidth;
        return fitsUpright || fitsRotated;
    }

    static class BinarySearch {
        int min, max, low, high, current;

        public BinarySearch (int min, int max) {
            this.min = getExponentNextOrMatchingPowerOfTwo(min);
            this.max = getExponentNextOrMatchingPowerOfTwo(max);
        }

        public int reset () {
            low = min;
            high = max;
            current = (low + high) >>> 1;
            return 1 << current;
        }

        public int next (boolean result) {
            if (low >= high) return -1;
            if (result)
                low = current + 1;
            else
                high = current - 1;
            current = (low + high) >>> 1;
            if (Math.abs(low - high) < 0) return -1;
            return 1 << current;
        }
    }

    static class RectNode {
        Rect rect;
        int score1;
        int score2;

        public RectNode() {
            this.rect = null;
            this.score1 = 0;
            this.score2 = 0;
        }

        public RectNode(Rect rect) {
            this.rect = new Rect(rect);
            this.score1 = 0;
            this.score2 = 0;
        }

        public RectNode(RectNode other) {
            set(other);
        }

        public void set(RectNode other) {
            this.rect = new Rect(other.rect);
            this.score1 = other.score1;
            this.score2 = other.score2;
        }
    }

    static class Page {
        public ArrayList<RectNode> outputRects, remainingRects;
        public float occupancy;
        public int width, height;
    }

    static class RectStats {
        final int maxWidth;
        final int maxHeight;
        final long totalArea;

        RectStats(int maxWidth, int maxHeight, long totalArea) {
            this.maxWidth = maxWidth;
            this.maxHeight = maxHeight;
            this.totalArea = totalArea;
        }
    }

    /** Maximal rectangles bin packing algorithm. Adapted from this C++ public domain source:
     * http://clb.demon.fi/projects/even-more-rectangle-bin-packing
     * @author Jukka Jyl�nki
     * @author Nathan Sweet */
    class MaxRects {
        private int binWidth;
        private int binHeight;
        private int newFreeRectanglesLastSize;
        private final ArrayList<RectNode> usedRectangles = new ArrayList<RectNode>();
        private final ArrayList<RectNode> freeRectangles = new ArrayList<RectNode>();
        private final ArrayList<RectNode> newFreeRectangles = new ArrayList<RectNode>();
        private final HashMap<Integer, ArrayList<RectNode>> verticalEdges = new HashMap<Integer, ArrayList<RectNode>>();
        private final HashMap<Integer, ArrayList<RectNode>> horizontalEdges = new HashMap<Integer, ArrayList<RectNode>>();

        public void init (int width, int height) {
            binWidth = width;
            binHeight = height;

            usedRectangles.clear();
            freeRectangles.clear();
            newFreeRectangles.clear();
            verticalEdges.clear();
            horizontalEdges.clear();
            RectNode n = new RectNode(new Rect(null, 0, 0, 0, width, height));
            freeRectangles.add(n);
        }

        /** Packs a single image. Order is defined externally. */
        public RectNode insert (RectNode rect, FreeRectChoiceHeuristic method) {
            RectNode newNode = scoreRect(rect, method);
            if (newNode.rect.getHeight() == 0) return null;

            int numRectanglesToProcess = freeRectangles.size();
            for (int i = 0; i < numRectanglesToProcess; ++i) {
                if (splitFreeNode(freeRectangles.get(i), newNode)) {
                    freeRectangles.remove(i);
                    --i;
                    --numRectanglesToProcess;
                }
            }

            pruneFreeList();

            RectNode bestNode = new RectNode(rect);
            bestNode.score1 = newNode.score1;
            bestNode.score2 = newNode.score2;
            bestNode.rect = new Rect(newNode.rect);
            bestNode.rect.setId(rect.rect.getId());
            bestNode.rect.setIndex(rect.rect.getIndex());

            addUsedRectangle(bestNode);
            return bestNode;
        }

        /** For each rectangle, packs each one then chooses the best and packs that. Slow! */
        public Page pack (ArrayList<RectNode> rects, FreeRectChoiceHeuristic method) {
            rects = new ArrayList<RectNode>(rects);
            while (!rects.isEmpty()) {
                int bestRectIndex = -1;
                RectNode bestNode = new RectNode(new Rect(null, 0, 0, 0, 0, 0));
                bestNode.score1 = Integer.MAX_VALUE;
                bestNode.score2 = Integer.MAX_VALUE;

                // Find the next rectangle that packs best.
                for (int i = 0; i < rects.size(); i++) {
                    RectNode newNode = scoreRect(rects.get(i), method);
                    if (newNode.score1 < bestNode.score1 || (newNode.score1 == bestNode.score1 && newNode.score2 < bestNode.score2)) {
                        bestNode.set(rects.get(i));
                        bestNode.score1 = newNode.score1;
                        bestNode.score2 = newNode.score2;
                        bestNode.rect.setX(newNode.rect.getX());
                        bestNode.rect.setY(newNode.rect.getY());
                        bestNode.rect.setWidth(newNode.rect.getWidth());
                        bestNode.rect.setHeight(newNode.rect.getHeight());
                        bestNode.rect.setRotated(newNode.rect.getRotated());
                        bestRectIndex = i;
                    }
                }

                if (bestRectIndex == -1) break;

                placeRect(bestNode);
                rects.remove(bestRectIndex);
            }

            Page result = getResult();
            result.remainingRects = rects;
            return result;
        }

        public Page getResult () {
            int w = 0, h = 0;
            for (int i = 0; i < usedRectangles.size(); i++) {
                RectNode node = usedRectangles.get(i);
                w = Math.max(w, node.rect.getX() + node.rect.getWidth());
                h = Math.max(h, node.rect.getY() + node.rect.getHeight());
            }
            Page result = new Page();
            result.outputRects = new ArrayList<RectNode>(usedRectangles);
            result.occupancy = getOccupancy();
            result.width = w;
            result.height = h;
            return result;
        }

        private void placeRect (RectNode node) {
            int numRectanglesToProcess = freeRectangles.size();
            for (int i = 0; i < numRectanglesToProcess;) {
                if (splitFreeNode(freeRectangles.get(i), node)) {
                    freeRectangles.remove(i);
                    --numRectanglesToProcess;
                }
                else
                {
                    ++i;
                }
            }

            pruneFreeList();

            addUsedRectangle(node);
        }

        private void addUsedRectangle(RectNode node) {
            usedRectangles.add(node);
            registerEdges(node);
        }

        private void registerEdges(RectNode node) {
            Rect rect = node.rect;
            addEdge(verticalEdges, rect.getX(), node);
            addEdge(verticalEdges, rect.getX() + rect.getWidth(), node);
            addEdge(horizontalEdges, rect.getY(), node);
            addEdge(horizontalEdges, rect.getY() + rect.getHeight(), node);
        }

        private void addEdge(HashMap<Integer, ArrayList<RectNode>> edges, int key, RectNode node) {
            ArrayList<RectNode> list = edges.get(key);
            if (list == null) {
                list = new ArrayList<RectNode>();
                edges.put(key, list);
            }
            list.add(node);
        }

        private RectNode scoreRect (RectNode node, FreeRectChoiceHeuristic method) {
            int width = node.rect.getWidth();
            int height = node.rect.getHeight();
            int rotatedWidth = height - settings.paddingY + settings.paddingX;
            int rotatedHeight = width - settings.paddingX + settings.paddingY;
            boolean rotate = /*node.rect.canRotate &&*/ settings.rotation;

            RectNode newNode = null;
            switch (method) {
            case BestShortSideFit:
                newNode = findPositionForNewNodeBestShortSideFit(width, height, rotatedWidth, rotatedHeight, rotate);
                break;
            case BottomLeftRule:
                newNode = findPositionForNewNodeBottomLeft(width, height, rotatedWidth, rotatedHeight, rotate);
                break;
            case ContactPointRule:
                newNode = findPositionForNewNodeContactPoint(width, height, rotatedWidth, rotatedHeight, rotate);
                newNode.score1 = -newNode.score1; // Reverse since we are minimizing, but for contact point score bigger is better.
                break;
            case BestLongSideFit:
                newNode = findPositionForNewNodeBestLongSideFit(width, height, rotatedWidth, rotatedHeight, rotate);
                break;
            case BestAreaFit:
                newNode = findPositionForNewNodeBestAreaFit(width, height, rotatedWidth, rotatedHeight, rotate);
                break;
            }

            // Cannot fit the current rectangle.
            if (newNode.rect.getHeight() == 0) {
                newNode.score1 = Integer.MAX_VALUE;
                newNode.score2 = Integer.MAX_VALUE;
            }

            return newNode;
        }

        // / Computes the ratio of used surface area.
        private float getOccupancy () {
            int usedSurfaceArea = 0;
            for (int i = 0; i < usedRectangles.size(); i++)
                usedSurfaceArea += usedRectangles.get(i).rect.getArea();
            return (float)usedSurfaceArea / (binWidth * binHeight);
        }

        private RectNode findPositionForNewNodeBottomLeft (int width, int height, int rotatedWidth, int rotatedHeight, boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0,0);
            bestNode.score1 = Integer.MAX_VALUE; // best y, score2 is best x

            boolean doRotate = rotate && width != height;

            for (int i = 0; i < freeRectangles.size(); i++) {
                // Try to place the rectangle in upright (non-rotated) orientation.
                RectNode currentNode = freeRectangles.get(i);
                if (currentNode.rect.getWidth() >= width && currentNode.rect.getHeight() >= height) {
                    int topSideY = currentNode.rect.getY() + height;
                    if (topSideY < bestNode.score1 || (topSideY == bestNode.score1 && currentNode.rect.getX() < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.getId(), currentNode.rect.getIndex(), currentNode.rect.getX(), currentNode.rect.getY(), width, height);
                        bestNode.score1 = topSideY;
                        bestNode.score2 = currentNode.rect.getX();
                    }
                }
                if (doRotate && currentNode.rect.getWidth() >= rotatedWidth && currentNode.rect.getHeight() >= rotatedHeight) {
                    int topSideY = currentNode.rect.getY() + rotatedHeight;
                    if (topSideY < bestNode.score1 || (topSideY == bestNode.score1 && currentNode.rect.getX() < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.getId(), currentNode.rect.getIndex(), currentNode.rect.getX(), currentNode.rect.getY(), rotatedWidth, rotatedHeight);
                        bestNode.score1 = topSideY;
                        bestNode.score2 = currentNode.rect.getX();
                        bestNode.rect.setRotated(true);
                    }
                }
            }
            return bestNode;
        }

        private RectNode findPositionForNewNodeBestShortSideFit (int width, int height, int rotatedWidth, int rotatedHeight,
            boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0,0);
            bestNode.score1 = Integer.MAX_VALUE;

            boolean doRotate = rotate && width != height;

            for (int i = 0; i < freeRectangles.size(); i++) {
                // Try to place the rectangle in upright (non-rotated) orientation.
                RectNode currentNode = freeRectangles.get(i);
                if (currentNode.rect.getWidth() >= width && currentNode.rect.getHeight() >= height) {
                    int leftoverHoriz = Math.abs(currentNode.rect.getWidth() - width);
                    int leftoverVert = Math.abs(currentNode.rect.getHeight() - height);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);
                    int longSideFit = Math.max(leftoverHoriz, leftoverVert);

                    if (shortSideFit < bestNode.score1 || (shortSideFit == bestNode.score1 && longSideFit < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.getId(), currentNode.rect.getIndex(), currentNode.rect.getX(), currentNode.rect.getY(), width, height);
                        bestNode.score1 = shortSideFit;
                        bestNode.score2 = longSideFit;
                    }
                }

                if (doRotate && currentNode.rect.getWidth() >= rotatedWidth && currentNode.rect.getHeight() >= rotatedHeight) {
                    int flippedLeftoverHoriz = Math.abs(currentNode.rect.getWidth() - rotatedWidth);
                    int flippedLeftoverVert = Math.abs(currentNode.rect.getHeight() - rotatedHeight);
                    int flippedShortSideFit = Math.min(flippedLeftoverHoriz, flippedLeftoverVert);
                    int flippedLongSideFit = Math.max(flippedLeftoverHoriz, flippedLeftoverVert);

                    if (flippedShortSideFit < bestNode.score1
                        || (flippedShortSideFit == bestNode.score1 && flippedLongSideFit < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.getId(), currentNode.rect.getIndex(), currentNode.rect.getX(), currentNode.rect.getY(), rotatedWidth, rotatedHeight);
                        bestNode.score1 = flippedShortSideFit;
                        bestNode.score2 = flippedLongSideFit;
                        bestNode.rect.setRotated(true);
                    }
                }
            }

            return bestNode;
        }

        private RectNode findPositionForNewNodeBestLongSideFit (int width, int height, int rotatedWidth, int rotatedHeight,
            boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0,0);
            bestNode.score2 = Integer.MAX_VALUE;

            for (int i = 0; i < freeRectangles.size(); i++) {
                // Try to place the rectangle in upright (non-rotated) orientation.
                RectNode currentNode = freeRectangles.get(i);
                if (currentNode.rect.getWidth() >= width && currentNode.rect.getHeight() >= height) {
                    int leftoverHoriz = Math.abs(currentNode.rect.getWidth() - width);
                    int leftoverVert = Math.abs(currentNode.rect.getHeight() - height);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);
                    int longSideFit = Math.max(leftoverHoriz, leftoverVert);

                    if (longSideFit < bestNode.score2 || (longSideFit == bestNode.score2 && shortSideFit < bestNode.score1)) {
                        bestNode.rect = new Rect(currentNode.rect.getId(), currentNode.rect.getIndex(), currentNode.rect.getX(), currentNode.rect.getY(), width, height);
                        bestNode.rect.setRotated(currentNode.rect.getRotated());
                        bestNode.score1 = shortSideFit;
                        bestNode.score2 = longSideFit;
                    }
                }

                if (rotate && currentNode.rect.getWidth() >= rotatedWidth && currentNode.rect.getHeight() >= rotatedHeight) {
                    int leftoverHoriz = Math.abs(currentNode.rect.getWidth() - rotatedWidth);
                    int leftoverVert = Math.abs(currentNode.rect.getHeight() - rotatedHeight);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);
                    int longSideFit = Math.max(leftoverHoriz, leftoverVert);

                    if (longSideFit < bestNode.score2 || (longSideFit == bestNode.score2 && shortSideFit < bestNode.score1)) {
                        bestNode.rect = new Rect(currentNode.rect.getId(), currentNode.rect.getIndex(), currentNode.rect.getX(), currentNode.rect.getY(), rotatedWidth, rotatedHeight);
                        bestNode.score1 = shortSideFit;
                        bestNode.score2 = longSideFit;
                        bestNode.rect.setRotated(true);
                    }
                }
            }
            return bestNode;
        }

        private RectNode findPositionForNewNodeBestAreaFit (int width, int height, int rotatedWidth, int rotatedHeight, boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0,0);
            bestNode.score1 = Integer.MAX_VALUE; // best area fit, score2 is best short side fit

            for (int i = 0; i < freeRectangles.size(); i++) {
                RectNode currentNode = freeRectangles.get(i);
                int areaFit = currentNode.rect.getArea() - width * height;

                // Try to place the rectangle in upright (non-rotated) orientation.
                if (currentNode.rect.getWidth() >= width && currentNode.rect.getHeight() >= height) {
                    int leftoverHoriz = Math.abs(currentNode.rect.getWidth() - width);
                    int leftoverVert = Math.abs(currentNode.rect.getHeight() - height);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);

                    if (areaFit < bestNode.score1 || (areaFit == bestNode.score1 && shortSideFit < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.getId(), currentNode.rect.getIndex(), currentNode.rect.getX(), currentNode.rect.getY(), width, height);
                        bestNode.score2 = shortSideFit;
                        bestNode.score1 = areaFit;
                    }
                }

                if (rotate && currentNode.rect.getWidth() >= rotatedWidth && currentNode.rect.getHeight() >= rotatedHeight) {
                    int leftoverHoriz = Math.abs(currentNode.rect.getWidth() - rotatedWidth);
                    int leftoverVert = Math.abs(currentNode.rect.getHeight() - rotatedHeight);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);

                    if (areaFit < bestNode.score1 || (areaFit == bestNode.score1 && shortSideFit < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.getId(), currentNode.rect.getIndex(), currentNode.rect.getX(), currentNode.rect.getY(), rotatedWidth, rotatedHeight);
                        bestNode.score2 = shortSideFit;
                        bestNode.score1 = areaFit;
                        bestNode.rect.setRotated(true);
                    }
                }
            }
            return bestNode;
        }

        // / Returns 0 if the two intervals i1 and i2 are disjoint, or the length of their overlap otherwise.
        private int commonIntervalLength(int i1start, int i1end, int i2start, int i2end) {
            if (i1end < i2start || i2end < i1start) return 0;
            return Math.min(i1end, i2end) - Math.max(i1start, i2start);
        }

        private int contactPointScoreNode(int x, int y, int width, int height) {
            int score = 0;

            if (x == 0 || x + width == binWidth) score += height;
            if (y == 0 || y + height == binHeight) score += width;

            ArrayList<RectNode> rightEdgeNodes = verticalEdges.get(x + width);
            if (rightEdgeNodes != null) {
                for (int i = 0; i < rightEdgeNodes.size(); i++) {
                    RectNode currentNode = rightEdgeNodes.get(i);
                    if (currentNode.rect.getX() == x + width) {
                        score += commonIntervalLength(currentNode.rect.getY(), currentNode.rect.getY() + currentNode.rect.getHeight(), y,
                            y + height);
                    }
                }
            }

            ArrayList<RectNode> leftEdgeNodes = verticalEdges.get(x);
            if (leftEdgeNodes != null) {
                for (int i = 0; i < leftEdgeNodes.size(); i++) {
                    RectNode currentNode = leftEdgeNodes.get(i);
                    if (currentNode.rect.getX() + currentNode.rect.getWidth() == x) {
                        score += commonIntervalLength(currentNode.rect.getY(), currentNode.rect.getY() + currentNode.rect.getHeight(), y,
                            y + height);
                    }
                }
            }

            ArrayList<RectNode> topEdgeNodes = horizontalEdges.get(y + height);
            if (topEdgeNodes != null) {
                for (int i = 0; i < topEdgeNodes.size(); i++) {
                    RectNode currentNode = topEdgeNodes.get(i);
                    if (currentNode.rect.getY() == y + height) {
                        score += commonIntervalLength(currentNode.rect.getX(), currentNode.rect.getX() + currentNode.rect.getWidth(), x, x
                            + width);
                    }
                }
            }

            ArrayList<RectNode> bottomEdgeNodes = horizontalEdges.get(y);
            if (bottomEdgeNodes != null) {
                for (int i = 0; i < bottomEdgeNodes.size(); i++) {
                    RectNode currentNode = bottomEdgeNodes.get(i);
                    if (currentNode.rect.getY() + currentNode.rect.getHeight() == y) {
                        score += commonIntervalLength(currentNode.rect.getX(), currentNode.rect.getX() + currentNode.rect.getWidth(), x, x
                            + width);
                    }
                }
            }
            return score;
        }

        private RectNode evaluateContactPoint(int width, int height, int rotatedWidth, int rotatedHeight, boolean rotate, Rect currRect) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0,0);
            bestNode.score1 = -1; // best contact score

            // Try to place the rectangle in upright (non-rotated) orientation.
            if (currRect.getWidth() >= width && currRect.getHeight() >= height) {
                int score = contactPointScoreNode(currRect.getX(), currRect.getY(), width, height);
                if (score > bestNode.score1) {
                    bestNode.rect = new Rect(currRect.getId(), currRect.getIndex(), currRect.getX(), currRect.getY(), width, height);
                    bestNode.score1 = score;
                }
            }

            if (rotate && currRect.getWidth() >= rotatedWidth && currRect.getHeight() >= rotatedHeight) {
            	// This was width,height -- bug fixed?
                int score = contactPointScoreNode(currRect.getX(), currRect.getY(), rotatedWidth, rotatedHeight);
                if (score > bestNode.score1) {
                    bestNode.rect = new Rect(currRect.getId(), currRect.getIndex(), currRect.getX(), currRect.getY(), rotatedWidth, rotatedHeight);
                    bestNode.score1 = score;
                    bestNode.rect.setRotated(true);
                }
            }

            return bestNode;
        }

        private RectNode findPositionForNewNodeContactPoint(int width, int height, int rotatedWidth, int rotatedHeight, boolean rotate) {
            RectNode globalBestNode = new RectNode();
            globalBestNode.rect = new Rect(null, 0,0,0,0,0);
            globalBestNode.score1 = Integer.MIN_VALUE; // maximize contact points

            if (freeRectangles.size() < 100) {
                // Small set: sequential scan
                for (RectNode currentNode : freeRectangles) {
                    RectNode candidate = evaluateContactPoint(width, height, rotatedWidth, rotatedHeight, rotate, currentNode.rect);
                    if (candidate.score1 > globalBestNode.score1) {
                        globalBestNode = candidate;
                    }
                }
            } else {
                // Big set: parallel scan
                globalBestNode = freeRectangles.parallelStream()
                    .map(currentNode -> evaluateContactPoint(width, height, rotatedWidth, rotatedHeight, rotate, currentNode.rect))
                    .reduce((best1, best2) -> (best1.score1 > best2.score1) ? best1 : best2)
                    .orElse(globalBestNode);
            }

            return globalBestNode;
        }

        private boolean splitFreeNode (RectNode freeNode, RectNode usedNode) {
            Rect freeRect = freeNode.rect;
            Rect usedRect = usedNode.rect;
            // Test with SAT if the rectangles even intersect.
            if (usedRect.getX() >= freeRect.getX() + freeRect.getWidth() || usedRect.getX() + usedRect.getWidth() <= freeRect.getX()
                || usedRect.getY() >= freeRect.getY() + freeRect.getHeight() || usedRect.getY() + usedRect.getHeight() <= freeRect.getY())
                    return false;

            // We add up to four new free rectangles to the free rectangles list below. None of these
            // four newly added free rectangles can overlap any other three, so keep a mark of them
            // to avoid testing them against each other.
            newFreeRectanglesLastSize = newFreeRectangles.size();

            if (usedRect.getX() < freeRect.getX() + freeRect.getWidth() && usedRect.getX() + usedRect.getWidth() > freeRect.getX()) {
                // New node at the top side of the used node.
                if (usedRect.getY() > freeRect.getY() && usedRect.getY() < freeRect.getY() + freeRect.getHeight()) {
                    RectNode newNode = new RectNode(freeNode);
                    newNode.rect.setHeight(usedRect.getY() - newNode.rect.getY());
                    insertNewFreeRectangle(newNode);
                }

                // New node at the bottom side of the used node.
                if (usedRect.getY() + usedRect.getHeight() < freeRect.getY() + freeRect.getHeight()) {
                    RectNode newNode = new RectNode(freeNode);
                    newNode.rect.setY(usedRect.getY() + usedRect.getHeight());
                    newNode.rect.setHeight(freeRect.getY() + freeRect.getHeight() - (usedRect.getY() + usedRect.getHeight()));
                    insertNewFreeRectangle(newNode);
                }
            }

            if (usedRect.getY() < freeRect.getY() + freeRect.getHeight() && usedRect.getY() + usedRect.getHeight() > freeRect.getY()) {
                // New node at the left side of the used node.
                if (usedRect.getX() > freeRect.getX() && usedRect.getX() < freeRect.getX() + freeRect.getWidth()) {
                    RectNode newNode = new RectNode(freeNode);
                    newNode.rect.setWidth(usedRect.getX() - newNode.rect.getX());
                    insertNewFreeRectangle(newNode);
                }

                // New node at the right side of the used node.
                if (usedRect.getX() + usedRect.getWidth() < freeRect.getX() + freeRect.getWidth()) {
                    RectNode newNode = new RectNode(freeNode);
                    newNode.rect.setX(usedRect.getX() + usedRect.getWidth());
                    newNode.rect.setWidth(freeRect.getX() + freeRect.getWidth() - (usedRect.getX() + usedRect.getWidth()));
                    insertNewFreeRectangle(newNode);
                }
            }

            return true;
        }

        private void insertNewFreeRectangle(RectNode newFreeRect)
        {
            for(int i = 0; i < newFreeRectanglesLastSize;)
            {
                // This new free rectangle is already accounted for?
                if (isContainedIn(newFreeRect.rect, newFreeRectangles.get(i).rect))
                    return;

                // Does this new free rectangle obsolete a previous new free rectangle?
                if (isContainedIn(newFreeRectangles.get(i).rect, newFreeRect.rect))
                {
                    // Remove i'th new free rectangle, but do so by retaining the order
                    // of the older vs newest free rectangles that we may still be placing
                    // in calling function SplitFreeNode().
                    newFreeRectangles.set(i, newFreeRectangles.get(--newFreeRectanglesLastSize));
                    newFreeRectangles.remove(newFreeRectanglesLastSize);
                }
                else
                {
                    ++i;
                }
            }
            newFreeRectangles.add(newFreeRect);
        }

        private void pruneFreeList () {
            // Test all newly introduced free rectangles against old free rectangles.
            for(int i = 0; i < freeRectangles.size(); ++i)
            {
                for(int j = 0; j < newFreeRectangles.size();)
                {
                    if (isContainedIn(newFreeRectangles.get(j).rect, freeRectangles.get(i).rect))
                    {
                        newFreeRectangles.remove(j);
                    }
                    else
                    {
                        ++j;
                    }
                }
            }

            // Merge new and old free rectangles to the group of old free rectangles.
            freeRectangles.addAll(newFreeRectangles);
            newFreeRectangles.clear();
        }

        private boolean isContainedIn (Rect a, Rect b) {
            return a.getX() >= b.getX() && a.getY() >= b.getY() && a.getX() + a.getWidth() <= b.getX() + b.getWidth() && a.getY() + a.getHeight() <= b.getY() + b.getHeight();
        }
    }

    static enum FreeRectChoiceHeuristic {
        // BSSF: Positions the rectangle against the short side of a free rectangle into which it fits the best.
        BestShortSideFit,
        // BLSF: Positions the rectangle against the long side of a free rectangle into which it fits the best.
        BestLongSideFit,
        // BAF: Positions the rectangle into the smallest free rect into which it fits.
        BestAreaFit,
        // BL: Does the Tetris placement.
        BottomLeftRule,
        // CP: Choosest the placement where the rectangle touches other rects as much as possible.
        ContactPointRule
    }
}
