package com.defold.editor.pipeline;

import java.util.ArrayList;
import java.util.List;

import com.defold.editor.pipeline.TextureSetLayout.Layout;
import com.defold.editor.pipeline.TextureSetLayout.Rect;

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
    public List<Layout> createLayout(List<Rect> srcRects) {
        ArrayList<RectNode> srcNodes = new ArrayList<RectNode>(srcRects.size());
        for(Rect r : srcRects) {
            RectNode n = new RectNode(r);
            n.rect.width += settings.paddingX;
            n.rect.height += settings.paddingY;
            srcNodes.add(n);
        }

        ArrayList<Page> pages = new ArrayList<Page>();
        while (srcNodes.size() > 0) {
            Page result = packPage(srcNodes);
            pages.add(result);
            srcNodes = result.remainingRects;
        }

        // Repackage into layouts.
        ArrayList<Layout> result = new ArrayList<Layout>(pages.size());
        for(Page page : pages) {
            ArrayList<Rect> rects = new ArrayList<Rect>(page.outputRects.size());
            for(RectNode node : page.outputRects) {
                Rect finalRect = new Rect(node.rect.id, node.rect.x, node.rect.y, node.rect.width - settings.paddingX, node.rect.height - settings.paddingY);
                finalRect.rotated = node.rect.rotated;
                rects.add(finalRect);
            }
            int layoutWidth = 1 << getExponentNextOrMatchingPowerOfTwo(page.width);
            int layoutHeight = 1 << getExponentNextOrMatchingPowerOfTwo(page.height);
            Layout layout = new Layout(layoutWidth, layoutHeight, rects);
            result.add(layout);
        }

        return result;
    }

    private Page packPage(ArrayList<RectNode> inputRects) {
        // Find min size.
        int minWidth = Integer.MAX_VALUE;
        int minHeight = Integer.MAX_VALUE;
        for (int i = 0, nn = inputRects.size(); i < nn; i++) {
            Rect rect = inputRects.get(i).rect;
            minWidth = Math.min(minWidth, rect.width);
            minHeight = Math.min(minHeight, rect.height);
            if (settings.rotation) {
                if ((rect.width > settings.maxPageWidth || rect.height > settings.maxPageHeight)
                    && (rect.width > settings.maxPageHeight || rect.height > settings.maxPageWidth)) {
                    throw new RuntimeException("Image does not fit with max page size " + settings.maxPageWidth + "x" + settings.maxPageHeight
                        + " and padding " + settings.paddingX + "," + settings.paddingY + ": " + rect);
                }
            } else {
                if (rect.width > settings.maxPageWidth) {
                    throw new RuntimeException("Image does not fit with max page width " + settings.maxPageWidth + " and paddingX "
                        + settings.paddingX + ": " + rect);
                }
                if (rect.height > settings.maxPageHeight && (!settings.rotation || rect.width > settings.maxPageHeight)) {
                    throw new RuntimeException("Image does not fit in max page height " + settings.maxPageHeight + " and paddingY "
                        + settings.paddingY + ": " + rect);
                }
            }
        }
        minWidth = Math.max(minWidth, settings.minPageWidth);
        minHeight = Math.max(minHeight, settings.minPageHeight);

        // Find the minimal page size that fits all rects.
        Page bestResult = null;
        if (settings.square) {
            int minSize = Math.max(minWidth, minHeight);
            int maxSize = Math.min(settings.maxPageWidth, settings.maxPageHeight);
            BinarySearch sizeSearch = new BinarySearch(minSize, maxSize);
            int size = sizeSearch.reset();
            while (size != -1) {
                Page result = packAtSize(true, size, size, inputRects);
                bestResult = getBest(bestResult, result);
                size = sizeSearch.next(result == null);
            }

            // Rects don't fit on one page. Fill a whole page and return.
            if (bestResult == null) {
                bestResult = packAtSize(false, maxSize, maxSize, inputRects);
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
                    Page result = packAtSize(true, width, height, inputRects);
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
                bestResult = packAtSize(false, settings.maxPageWidth, settings.maxPageHeight, inputRects);
            }
        }
        return bestResult;
    }

    /** @param fully If true, the only results that pack all rects will be considered. If false, all results are considered, not all
     *           rects may be packed.
     **/
    private Page packAtSize (boolean fully, int width, int height, ArrayList<RectNode> inputRects) {
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

            if (fully && result.remainingRects.size() > 0) {
                continue;
            }
            if (result.outputRects.size() == 0) {
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

        public RectNode(Rect rect, int score1, int score2) {
            this.rect = new Rect(rect);
            this.score1 = score1;
            this.score2 = score2;
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

    /** Maximal rectangles bin packing algorithm. Adapted from this C++ public domain source:
     * http://clb.demon.fi/projects/even-more-rectangle-bin-packing
     * @author Jukka Jyl�nki
     * @author Nathan Sweet */
    class MaxRects {
        private int binWidth;
        private int binHeight;
        private final ArrayList<RectNode> usedRectangles = new ArrayList<RectNode>();
        private final ArrayList<RectNode> freeRectangles = new ArrayList<RectNode>();

        public void init (int width, int height) {
            binWidth = width;
            binHeight = height;

            usedRectangles.clear();
            freeRectangles.clear();
            RectNode n = new RectNode(new Rect(null, 0, 0, width, height));
            freeRectangles.add(n);
        }

        /** Packs a single image. Order is defined externally. */
        public RectNode insert (RectNode rect, FreeRectChoiceHeuristic method) {
            RectNode newNode = scoreRect(rect, method);
            if (newNode.rect.height == 0) return null;

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
            bestNode.rect.id = rect.rect.id;

            usedRectangles.add(bestNode);
            return bestNode;
        }

        /** For each rectangle, packs each one then chooses the best and packs that. Slow! */
        public Page pack (ArrayList<RectNode> rects, FreeRectChoiceHeuristic method) {
            rects = new ArrayList<RectNode>(rects);
            while (rects.size() > 0) {
                int bestRectIndex = -1;
                RectNode bestNode = new RectNode(new Rect(null, 0, 0, 0, 0));
                bestNode.score1 = Integer.MAX_VALUE;
                bestNode.score2 = Integer.MAX_VALUE;

                // Find the next rectangle that packs best.
                for (int i = 0; i < rects.size(); i++) {
                    RectNode newNode = scoreRect(rects.get(i), method);
                    if (newNode.score1 < bestNode.score1 || (newNode.score1 == bestNode.score1 && newNode.score2 < bestNode.score2)) {
                        bestNode.set(rects.get(i));
                        bestNode.score1 = newNode.score1;
                        bestNode.score2 = newNode.score2;
                        bestNode.rect.x = newNode.rect.x;
                        bestNode.rect.y = newNode.rect.y;
                        bestNode.rect.width = newNode.rect.width;
                        bestNode.rect.height = newNode.rect.height;
                        bestNode.rect.rotated = newNode.rect.rotated;
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
                w = Math.max(w, node.rect.x + node.rect.width);
                h = Math.max(h, node.rect.y + node.rect.height);
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
            for (int i = 0; i < numRectanglesToProcess; i++) {
                if (splitFreeNode(freeRectangles.get(i), node)) {
                    freeRectangles.remove(i);
                    --i;
                    --numRectanglesToProcess;
                }
            }

            pruneFreeList();

            usedRectangles.add(node);
        }

        private RectNode scoreRect (RectNode node, FreeRectChoiceHeuristic method) {
            int width = node.rect.width;
            int height = node.rect.height;
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
            if (newNode.rect.height == 0) {
                newNode.score1 = Integer.MAX_VALUE;
                newNode.score2 = Integer.MAX_VALUE;
            }

            return newNode;
        }

        // / Computes the ratio of used surface area.
        private float getOccupancy () {
            int usedSurfaceArea = 0;
            for (int i = 0; i < usedRectangles.size(); i++)
                usedSurfaceArea += usedRectangles.get(i).rect.area();
            return (float)usedSurfaceArea / (binWidth * binHeight);
        }

        private RectNode findPositionForNewNodeBottomLeft (int width, int height, int rotatedWidth, int rotatedHeight, boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0);
            bestNode.score1 = Integer.MAX_VALUE; // best y, score2 is best x

            for (int i = 0; i < freeRectangles.size(); i++) {
                // Try to place the rectangle in upright (non-rotated) orientation.
                RectNode currentNode = freeRectangles.get(i);
                if (currentNode.rect.width >= width && currentNode.rect.height >= height) {
                    int topSideY = currentNode.rect.y + height;
                    if (topSideY < bestNode.score1 || (topSideY == bestNode.score1 && currentNode.rect.x < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, width, height);
                        bestNode.score1 = topSideY;
                        bestNode.score2 = currentNode.rect.x;
                    }
                }
                if (rotate && currentNode.rect.width >= rotatedWidth && currentNode.rect.height >= rotatedHeight) {
                    int topSideY = currentNode.rect.y + rotatedHeight;
                    if (topSideY < bestNode.score1 || (topSideY == bestNode.score1 && currentNode.rect.x < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, rotatedWidth, rotatedHeight);
                        bestNode.score1 = topSideY;
                        bestNode.score2 = currentNode.rect.x;
                        bestNode.rect.rotated = true;
                    }
                }
            }
            return bestNode;
        }

        private RectNode findPositionForNewNodeBestShortSideFit (int width, int height, int rotatedWidth, int rotatedHeight,
            boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0);
            bestNode.score1 = Integer.MAX_VALUE;

            for (int i = 0; i < freeRectangles.size(); i++) {
                // Try to place the rectangle in upright (non-rotated) orientation.
                RectNode currentNode = freeRectangles.get(i);
                if (currentNode.rect.width >= width && currentNode.rect.height >= height) {
                    int leftoverHoriz = Math.abs(currentNode.rect.width - width);
                    int leftoverVert = Math.abs(currentNode.rect.height - height);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);
                    int longSideFit = Math.max(leftoverHoriz, leftoverVert);

                    if (shortSideFit < bestNode.score1 || (shortSideFit == bestNode.score1 && longSideFit < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, width, height);
                        bestNode.score1 = shortSideFit;
                        bestNode.score2 = longSideFit;
                    }
                }

                if (rotate && currentNode.rect.width >= rotatedWidth && currentNode.rect.height >= rotatedHeight) {
                    int flippedLeftoverHoriz = Math.abs(currentNode.rect.width - rotatedWidth);
                    int flippedLeftoverVert = Math.abs(currentNode.rect.height - rotatedHeight);
                    int flippedShortSideFit = Math.min(flippedLeftoverHoriz, flippedLeftoverVert);
                    int flippedLongSideFit = Math.max(flippedLeftoverHoriz, flippedLeftoverVert);

                    if (flippedShortSideFit < bestNode.score1
                        || (flippedShortSideFit == bestNode.score1 && flippedLongSideFit < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, rotatedWidth, rotatedHeight);
                        bestNode.score1 = flippedShortSideFit;
                        bestNode.score2 = flippedLongSideFit;
                        bestNode.rect.rotated = true;
                    }
                }
            }

            return bestNode;
        }

        private RectNode findPositionForNewNodeBestLongSideFit (int width, int height, int rotatedWidth, int rotatedHeight,
            boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0);
            bestNode.score2 = Integer.MAX_VALUE;

            for (int i = 0; i < freeRectangles.size(); i++) {
                // Try to place the rectangle in upright (non-rotated) orientation.
                RectNode currentNode = freeRectangles.get(i);
                if (currentNode.rect.width >= width && currentNode.rect.height >= height) {
                    int leftoverHoriz = Math.abs(currentNode.rect.width - width);
                    int leftoverVert = Math.abs(currentNode.rect.height - height);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);
                    int longSideFit = Math.max(leftoverHoriz, leftoverVert);

                    if (longSideFit < bestNode.score2 || (longSideFit == bestNode.score2 && shortSideFit < bestNode.score1)) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, width, height);
                        bestNode.rect.rotated = currentNode.rect.rotated;
                        bestNode.score1 = shortSideFit;
                        bestNode.score2 = longSideFit;
                    }
                }

                if (rotate && currentNode.rect.width >= rotatedWidth && currentNode.rect.height >= rotatedHeight) {
                    int leftoverHoriz = Math.abs(currentNode.rect.width - rotatedWidth);
                    int leftoverVert = Math.abs(currentNode.rect.height - rotatedHeight);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);
                    int longSideFit = Math.max(leftoverHoriz, leftoverVert);

                    if (longSideFit < bestNode.score2 || (longSideFit == bestNode.score2 && shortSideFit < bestNode.score1)) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, rotatedWidth, rotatedHeight);
                        bestNode.score1 = shortSideFit;
                        bestNode.score2 = longSideFit;
                        bestNode.rect.rotated = true;
                    }
                }
            }
            return bestNode;
        }

        private RectNode findPositionForNewNodeBestAreaFit (int width, int height, int rotatedWidth, int rotatedHeight, boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0);
            bestNode.score1 = Integer.MAX_VALUE; // best area fit, score2 is best short side fit

            for (int i = 0; i < freeRectangles.size(); i++) {
                RectNode currentNode = freeRectangles.get(i);
                int areaFit = currentNode.rect.area() - width * height;

                // Try to place the rectangle in upright (non-rotated) orientation.
                if (currentNode.rect.width >= width && currentNode.rect.height >= height) {
                    int leftoverHoriz = Math.abs(currentNode.rect.width - width);
                    int leftoverVert = Math.abs(currentNode.rect.height - height);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);

                    if (areaFit < bestNode.score1 || (areaFit == bestNode.score1 && shortSideFit < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, width, height);
                        bestNode.score2 = shortSideFit;
                        bestNode.score1 = areaFit;
                    }
                }

                if (rotate && currentNode.rect.width >= rotatedWidth && currentNode.rect.height >= rotatedHeight) {
                    int leftoverHoriz = Math.abs(currentNode.rect.width - rotatedWidth);
                    int leftoverVert = Math.abs(currentNode.rect.height - rotatedHeight);
                    int shortSideFit = Math.min(leftoverHoriz, leftoverVert);

                    if (areaFit < bestNode.score1 || (areaFit == bestNode.score1 && shortSideFit < bestNode.score2)) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, rotatedWidth, rotatedHeight);
                        bestNode.score2 = shortSideFit;
                        bestNode.score1 = areaFit;
                        bestNode.rect.rotated = true;
                    }
                }
            }
            return bestNode;
        }

        // / Returns 0 if the two intervals i1 and i2 are disjoint, or the length of their overlap otherwise.
        private int commonIntervalLength (int i1start, int i1end, int i2start, int i2end) {
            if (i1end < i2start || i2end < i1start) return 0;
            return Math.min(i1end, i2end) - Math.max(i1start, i2start);
        }

        private int contactPointScoreNode (int x, int y, int width, int height) {
            int score = 0;

            if (x == 0 || x + width == binWidth) score += height;
            if (y == 0 || y + height == binHeight) score += width;

            for (int i = 0; i < usedRectangles.size(); i++) {
                RectNode currentNode = usedRectangles.get(i);
                if (currentNode.rect.x == x + width || currentNode.rect.x + currentNode.rect.width == x)
                    score += commonIntervalLength(currentNode.rect.y, currentNode.rect.y + currentNode.rect.height, y,
                        y + height);
                if (currentNode.rect.y == y + height || currentNode.rect.y + currentNode.rect.height == y)
                    score += commonIntervalLength(currentNode.rect.x, currentNode.rect.x + currentNode.rect.width, x, x
                        + width);
            }
            return score;
        }

        private RectNode findPositionForNewNodeContactPoint (int width, int height, int rotatedWidth, int rotatedHeight, boolean rotate) {
            RectNode bestNode = new RectNode();
            bestNode.rect = new Rect(null, 0,0,0,0);
            bestNode.score1 = -1; // best contact score

            for (int i = 0; i < freeRectangles.size(); i++) {
                // Try to place the rectangle in upright (non-rotated) orientation.
                RectNode currentNode = freeRectangles.get(i);
                if (currentNode.rect.width >= width && currentNode.rect.height >= height) {
                    int score = contactPointScoreNode(currentNode.rect.x, currentNode.rect.y, width, height);
                    if (score > bestNode.score1) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, width, height);
                        bestNode.score1 = score;
                    }
                }
                if (rotate && currentNode.rect.width >= rotatedWidth && currentNode.rect.height >= rotatedHeight) {
                    // This was width,height -- bug fixed?
                    int score = contactPointScoreNode(currentNode.rect.x, currentNode.rect.y, rotatedWidth, rotatedHeight);
                    if (score > bestNode.score1) {
                        bestNode.rect = new Rect(currentNode.rect.id, currentNode.rect.x, currentNode.rect.y, rotatedWidth, rotatedHeight);
                        bestNode.score1 = score;
                        bestNode.rect.rotated = true;
                    }
                }
            }
            return bestNode;
        }

        private boolean splitFreeNode (RectNode freeNode, RectNode usedNode) {
            Rect freeRect = freeNode.rect;
            Rect usedRect = usedNode.rect;
            // Test with SAT if the rectangles even intersect.
            if (usedRect.x >= freeRect.x + freeRect.width || usedRect.x + usedRect.width <= freeRect.x
                || usedRect.y >= freeRect.y + freeRect.height || usedRect.y + usedRect.height <= freeRect.y) return false;

            if (usedRect.x < freeRect.x + freeRect.width && usedRect.x + usedRect.width > freeRect.x) {
                // New node at the top side of the used node.
                if (usedRect.y > freeRect.y && usedRect.y < freeRect.y + freeRect.height) {
                    RectNode newNode = new RectNode(freeNode);
                    newNode.rect.height = usedRect.y - newNode.rect.y;
                    freeRectangles.add(newNode);
                }

                // New node at the bottom side of the used node.
                if (usedRect.y + usedRect.height < freeRect.y + freeRect.height) {
                    RectNode newNode = new RectNode(freeNode);
                    newNode.rect.y = usedRect.y + usedRect.height;
                    newNode.rect.height = freeRect.y + freeRect.height - (usedRect.y + usedRect.height);
                    freeRectangles.add(newNode);
                }
            }

            if (usedRect.y < freeRect.y + freeRect.height && usedRect.y + usedRect.height > freeRect.y) {
                // New node at the left side of the used node.
                if (usedRect.x > freeRect.x && usedRect.x < freeRect.x + freeRect.width) {
                    RectNode newNode = new RectNode(freeNode);
                    newNode.rect.width = usedRect.x - newNode.rect.x;
                    freeRectangles.add(newNode);
                }

                // New node at the right side of the used node.
                if (usedRect.x + usedRect.width < freeRect.x + freeRect.width) {
                    RectNode newNode = new RectNode(freeNode);
                    newNode.rect.x = usedRect.x + usedRect.width;
                    newNode.rect.width = freeRect.x + freeRect.width - (usedRect.x + usedRect.width);
                    freeRectangles.add(newNode);
                }
            }

            return true;
        }

        private void pruneFreeList () {
            /*
             * /// Would be nice to do something like this, to avoid a Theta(n^2) loop through each pair. /// But unfortunately it
             * doesn't quite cut it, since we also want to detect containment. /// Perhaps there's another way to do this faster than
             * Theta(n^2).
             *
             * if (freeRectangles.size > 0) clb::sort::QuickSort(&freeRectangles[0], freeRectangles.size, NodeSortCmp);
             *
             * for(int i = 0; i < freeRectangles.size-1; i++) if (freeRectangles[i].x == freeRectangles[i+1].x && freeRectangles[i].y
             * == freeRectangles[i+1].y && freeRectangles[i].width == freeRectangles[i+1].width && freeRectangles[i].height ==
             * freeRectangles[i+1].height) { freeRectangles.erase(freeRectangles.begin() + i); --i; }
             */

            // / Go through each pair and remove any rectangle that is redundant.
            for (int i = 0; i < freeRectangles.size(); i++)
                for (int j = i + 1; j < freeRectangles.size(); ++j) {
                    if (isContainedIn(freeRectangles.get(i).rect, freeRectangles.get(j).rect)) {
                        freeRectangles.remove(i);
                        --i;
                        break;
                    }
                    if (isContainedIn(freeRectangles.get(j).rect, freeRectangles.get(i).rect)) {
                        freeRectangles.remove(j);
                        --j;
                    }
                }
        }

        private boolean isContainedIn (Rect a, Rect b) {
            return a.x >= b.x && a.y >= b.y && a.x + a.width <= b.x + b.width && a.y + a.height <= b.y + b.height;
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
    };
}
