package com.dynamo.cr.parted.curve;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import org.eclipse.jface.resource.ColorRegistry;
import org.eclipse.jface.viewers.IColorProvider;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredContentProvider;
import org.eclipse.jface.viewers.ITreeSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.TreePath;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseWheelListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Font;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Menu;

import com.dynamo.cr.parted.curve.ICurveView.IPresenter;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.dynamo.cr.sceneed.core.util.CameraUtil;
import com.dynamo.cr.sceneed.core.util.CameraUtil.Movement;
import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;

/**
 * Curve Editor
 * @author chmu
 */

public class CurveViewer extends Canvas implements PaintListener,
        MouseWheelListener, MouseMoveListener, MouseListener, ISelectionProvider {

    public static final String BACKGROUND_COLOR_KEY = "com.dynamo.cr.parted.curve.BACKGROUND_COLOR";
    public static final String AXIS_COLOR_KEY = "com.dynamo.cr.parted.curve.AXIS_COLOR";
    public static final String GRID_COLOR_KEY = "com.dynamo.cr.parted.curve.GRID_COLOR";
    public static final String TICK_COLOR_KEY = "com.dynamo.cr.parted.curve.TICK_COLOR";
    public static final String TANGENT_COLOR_KEY = "com.dynamo.cr.parted.curve.TANGENT_COLOR";
    public static final String CONTROL_COLOR_KEY = "com.dynamo.cr.parted.curve.CONTROL_COLOR";
    public static final String SPLINE_COLOR_KEY = "com.dynamo.cr.parted.curve.SPLINE_COLOR";
    public static final String SELECTION_COLOR_KEY = "com.dynamo.cr.parted.curve.SELECTION_COLOR";
    public static final String SELECTION_BOX_COLOR_KEY = "com.dynamo.cr.parted.curve.SELECTION_BOX_COLOR";

    private static final int SCREEN_DRAG_PADDING = 2;
    private static final int SCREEN_HIT_PADDING = 5;
    private static final int SCREEN_TANGENT_LENGTH = 55;

    private static final int Y_AXIS_WIDTH = 60;
    private static final int MARGIN_RIGHT = 32;
    private static final int TICK_HEIGHT = 12;
    private static final int CONTROL_SIZE = 6;
    private static final int TANGENT_CONTROL_SIZE = 6;
    private static final int SUB_DIVISIONS = 55;

    private double offsetY = 0;
    private double zoomY = 100;
    private int prevY = -1;
    private Movement cameraMovement = Movement.IDLE;
    private boolean dragging = false;
    private Font font;
    private Menu contextMenu;

    private IStructuredContentProvider contentProvider;
    private ICurveProvider provider;
    private IColorProvider colorProvider;
    private ICurveView.IPresenter presenter;
    private Object input;
    private ISelection selection = new StructuredSelection();
    private Rectangle selectionBox = new Rectangle(0, 0, 0, 0);

    private ColorRegistry colorsRegistry;

    private Color getColor(String key) {
        return colorsRegistry.get(key);
    }

    public CurveViewer(Composite parent, int style, ColorRegistry colorRegistry) {
        super(parent, style | SWT.DOUBLE_BUFFERED);
        addPaintListener(this);
        addMouseWheelListener(this);
        addMouseMoveListener(this);
        addMouseListener(this);
        Font currentFont = getFont();
        font = new Font(getDisplay(), currentFont.getFontData()[0].getName(),
                10, SWT.NORMAL);
        setFont(font);
        this.colorsRegistry = colorRegistry;
        initColors();

        setBackground(getColor(BACKGROUND_COLOR_KEY));

    }

    public void setInput(Object input) {
        this.input = input;
    }

    public void setSelection(ISelection selection) {
        this.selection = selection;
    }

    public void setSelectionBox(Point2d min, Point2d max) {
        this.selectionBox = toScreenRect(min, max);
    }

    public void setContentProvider(IStructuredContentProvider contentProvider) {
        this.contentProvider = contentProvider;
    }

    public void setProvider(ICurveProvider provider) {
        this.provider = provider;
    }

    public void setColorProvider(IColorProvider colorProvider) {
        this.colorProvider = colorProvider;
    }

    public void setPresenter(IPresenter presenter) {
        this.presenter = presenter;
    }

    public void setContextMenu(Menu contextMenu) {
        this.contextMenu = contextMenu;
    }

    private void putColor(String key, RGB rgb) {
        if (colorsRegistry.get(key) == null) {
            colorsRegistry.put(key, rgb);
        }
    }

    public void addPoint(Point position) {
        this.presenter.onAddPoint(fromScreen(position));
    }

    private void initColors() {
        putColor(BACKGROUND_COLOR_KEY, new RGB(255, 255, 255));
        putColor(AXIS_COLOR_KEY, new RGB(100, 100, 100));
        putColor(GRID_COLOR_KEY, new RGB(220, 220, 220));
        putColor(TICK_COLOR_KEY, new RGB(100, 100, 100));
        putColor(TANGENT_COLOR_KEY, new RGB(0, 0x88, 0xcc));
        putColor(CONTROL_COLOR_KEY, new RGB(0xff, 0x24, 0x1e));
        putColor(SPLINE_COLOR_KEY, new RGB(0xcc, 0x00, 0x2f));
        putColor(SELECTION_COLOR_KEY, new RGB(0xff, 0xff, 0x00));
        float[] selectionColor = RenderUtil.parseColor(PreferenceConstants.P_SELECTION_COLOR);
        putColor(SELECTION_BOX_COLOR_KEY, new RGB((int)(selectionColor[0] * 255.0f), (int)(selectionColor[1] * 255.0f), (int)(selectionColor[2] * 255.0f)));
    }

    @Override
    public void dispose() {
        removePaintListener(this);
        removeMouseWheelListener(this);
        removeMouseMoveListener(this);
        removeMouseListener(this);
        font.dispose();
        super.dispose();
    }

    int getPlotWidth() {
        Point size = getSize();
        return size.x - Y_AXIS_WIDTH - MARGIN_RIGHT;
    }

    int getPlotHeight() {
        Point size = getSize();
        return size.y;
    }

    double toScreenX(double xValue) {
        return xValue * getPlotWidth() + Y_AXIS_WIDTH;
    }

    double toScreenY(double yValue) {
        return getPlotHeight() - yValue * zoomY + offsetY;
    }

    double fromScreenX(int xValue) {
        return (xValue - Y_AXIS_WIDTH) / (double) getPlotWidth();
    }

    double fromScreenY(int yValue) {
        return (getPlotHeight() - yValue + offsetY) / zoomY;
    }

    Point toScreen(Point2d p) {
        return new Point((int)Math.round(toScreenX(p.x)), (int)Math.round(toScreenY(p.y)));
    }

    Point toScreenDelta(Vector2d d) {
        int x = (int)Math.round(d.getX() * getPlotWidth());
        int y = (int)Math.round(-d.getY() * zoomY);
        return new Point(x, y);
    }

    Rectangle toScreenRect(Point2d min, Point2d max) {
        Point p0 = toScreen(min);
        Point p1 = toScreen(max);
        return new Rectangle(p0.x, p1.y, p1.x - p0.x, p0.y - p1.y);
    }

    Point2d fromScreen(Point p) {
        Point size = getSize();
        int plotWidth = size.x - Y_AXIS_WIDTH - MARGIN_RIGHT;
        int plotHeight = size.y;
        int y = plotHeight - p.y;
        return new Point2d((p.x - Y_AXIS_WIDTH) / (double) plotWidth, (y + offsetY) / zoomY);
    }

    Vector2d fromScreenDelta(Point p) {
        return new Vector2d(p.x / (double)getPlotWidth(), -p.y / zoomY);
    }

    private Map<Integer, List<Integer>> getSelectedPoints() {
        Map<Integer, List<Integer>> points = new HashMap<Integer, List<Integer>>();
        if (this.selection instanceof ITreeSelection) {
            ITreeSelection treeSelection = (ITreeSelection)this.selection;
            TreePath[] paths = treeSelection.getPaths();
            for (TreePath path : paths) {
                int curveIndex = (Integer)path.getSegment(0);
                List<Integer> p = points.get(curveIndex);
                if (p == null) {
                    p = new ArrayList<Integer>();
                    points.put(curveIndex, p);
                }
                if (path.getSegmentCount() > 1) {
                    p.add((Integer)path.getSegment(1));
                }
            }
        }
        return points;
    }

    private void drawGrid(GC gc) {
        Point size = getSize();
        int plotWidth = getPlotWidth();

        for (int i = 0; i < 10 + 1; i++) {
            String s = String.format("%d", i * 10);
            Point extents = gc.stringExtent(s);

            gc.setForeground(getColor(GRID_COLOR_KEY));
            int x = Y_AXIS_WIDTH + i * plotWidth / 10;
            int y = (int) toScreenY(0) + TICK_HEIGHT / 2 + 2; // + 2 for some
                                                             // extra margin
            gc.drawLine(x, 0, x, size.y);

            gc.setForeground(getColor(TICK_COLOR_KEY));
            gc.drawString(s, x - extents.x / 2, y);
            int tickX = Y_AXIS_WIDTH + i * plotWidth / 10;
            int tickY = (int) toScreenY(0);
            gc.drawLine(tickX, tickY - TICK_HEIGHT / 2, tickX, tickY
                    + TICK_HEIGHT / 2);
        }

        double maxY = fromScreenY(0);
        double minY = fromScreenY(size.y);
        int pixelStepY = 64;
        double yStep = fromScreenY(0) - fromScreenY(pixelStepY);
        yStep = Math.pow(10, Math.round(Math.log10(yStep)));

        int actualPixelStep = (int) (toScreenY(0) - toScreenY(yStep));

        if (actualPixelStep < pixelStepY) {
            yStep *= 2;
        }

        if (actualPixelStep > pixelStepY * 2) {
            yStep *= 0.5;
        }

        double startY = Math.floor(minY / yStep - 1) * yStep;
        double endY = (Math.ceil(maxY / yStep) + 1) * yStep;
        double valueY = startY;

        while (valueY < endY) {
            String s = String.format("%.2f", valueY);
            Point extents = gc.stringExtent(s);
            int x = Y_AXIS_WIDTH;
            int y = (int) toScreenY(valueY);

            gc.setForeground(getColor(GRID_COLOR_KEY));
            gc.drawLine(Y_AXIS_WIDTH, y, size.x, y);

            gc.setForeground(getColor(TICK_COLOR_KEY));
            gc.drawString(s, Y_AXIS_WIDTH - extents.x - 10, y - extents.y / 2);
            gc.drawLine(x - TICK_HEIGHT / 2, y, x + TICK_HEIGHT / 2, y);
            valueY += yStep;
        }
    }

    private void drawSpline(GC gc, HermiteSpline spline, Color splineColor, List<Integer> selectedPoints, int selectedCurveCount) {
        int segCount = spline.getSegmentCount();
        double[] value = new double[2];
        double[] prevValue = new double[2];
        gc.setForeground(splineColor);
        int selectedPointCount = selectedPoints != null ? selectedPoints.size() : 0;

        int[] points = new int[SUB_DIVISIONS * 2];
        for (int s = 0; s < segCount; s++) {
            spline.getValue(s, 0, prevValue);
            points[0] = (int) toScreenX(prevValue[0]);
            points[1] = (int) toScreenY(prevValue[1]);
            for (int i = 1; i < SUB_DIVISIONS; ++i) {
                double t = i / (double) (SUB_DIVISIONS - 1);
                spline.getValue(s, t, value);
                points[i * 2] = (int) toScreenX(value[0]);
                points[i * 2 + 1] = (int) toScreenY(value[1]);
                double[] tmp = prevValue;
                prevValue = value;
                value = tmp;
            }
            gc.drawPolyline(points);
        }

        int nPoints = spline.getCount();
        for (int i = 0; i < nPoints; ++i) {
            SplinePoint p = spline.getPoint(i);
            boolean selected = selectedPoints != null && selectedPoints.contains(i);

            int x = (int) toScreenX(p.getX());
            int y = (int) toScreenY(p.getY());
            if (selectedCurveCount == 1 && selectedPointCount == 1 && selected) {
                Vector2d screenTangent = new Vector2d(p.getTx() * getPlotWidth(), p.getTy() * -zoomY);
                screenTangent.normalize();
                screenTangent.scale(SCREEN_TANGENT_LENGTH);
                int tx = (int)Math.round(screenTangent.getX());
                int ty = (int)Math.round(screenTangent.getY());
                int x0 = x - tx;
                int y0 = y - ty;
                int x1 = x + tx;
                int y1 = y + ty;

                gc.setForeground(getColor(TANGENT_COLOR_KEY));
                gc.setBackground(getColor(TANGENT_COLOR_KEY));
                gc.drawLine(x0, y0, x1, y1);

                gc.fillOval(x0 - TANGENT_CONTROL_SIZE / 2, y0 - TANGENT_CONTROL_SIZE
                        / 2, TANGENT_CONTROL_SIZE, TANGENT_CONTROL_SIZE);

                gc.fillOval(x1 - TANGENT_CONTROL_SIZE / 2, y1 - TANGENT_CONTROL_SIZE
                        / 2, TANGENT_CONTROL_SIZE, TANGENT_CONTROL_SIZE);
            }

            if (selected) {
                gc.setForeground(getColor(SELECTION_COLOR_KEY));
            } else {
                gc.setForeground(getColor(CONTROL_COLOR_KEY));
            }
            gc.setBackground(getColor(BACKGROUND_COLOR_KEY));
            gc.fillOval(x - CONTROL_SIZE / 2, y - CONTROL_SIZE / 2, CONTROL_SIZE,
                    CONTROL_SIZE);
            gc.drawOval(x - CONTROL_SIZE / 2, y - CONTROL_SIZE / 2, CONTROL_SIZE,
                    CONTROL_SIZE);

        }
    }

    private void drawSelectionBox(GC gc) {
        if (!this.selectionBox.isEmpty()) {
            gc.setAlpha(255);
            gc.setForeground(getColor(SELECTION_BOX_COLOR_KEY));
            gc.setBackground(getColor(SELECTION_BOX_COLOR_KEY));
            gc.drawRectangle(this.selectionBox);
            gc.setAlpha(50);
            gc.fillRectangle(this.selectionBox);
            gc.setAlpha(255);
        }
    }

    @Override
    public void paintControl(PaintEvent e) {
        if (!isEnabled())
            return;

        GC gc = e.gc;
        drawGrid(gc);
        Map<Integer, List<Integer>> selectedPoints = getSelectedPoints();
        int selectedCurveCount = selectedPoints.size();
        Object[] elements = this.contentProvider.getElements(input);
        for (int i = 0; i < elements.length; ++i) {
            if (!provider.isEnabled(i))
                continue;

            HermiteSpline spline = (HermiteSpline)this.provider.getSpline(i);
            Color color = null;
            if (selectedPoints.containsKey(i)) {
                color = getColor(SELECTION_COLOR_KEY);
            }
            if (color == null) {
                color = this.colorProvider.getForeground(elements[i]);
            }
            if (color == null) {
                color = getColor(SPLINE_COLOR_KEY);
            }
            drawSpline(gc, spline, color, selectedPoints.get(i), selectedCurveCount);
        }
        drawSelectionBox(gc);
    }

    private void dolly(double amount) {
        Point size = getSize();

        double center = (fromScreenY(0) + fromScreenY(size.y)) * 0.5;
        double screenCenter0 = toScreenY(center);

        zoomY += amount * zoomY;
        zoomY = Math.max(0.01, zoomY);
        double screenCenter1 = toScreenY(center);
        offsetY += (screenCenter0 - screenCenter1);
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (provider == null)
            return;

        int dy = e.y - prevY;
        switch (cameraMovement) {
        case TRACK:
            offsetY += dy;
            redraw();
            break;
        case DOLLY:
            dolly(-dy * 0.005);
            redraw();
            break;
        default:
            if ((e.stateMask & SWT.BUTTON1) != 0) {
                this.presenter.onDrag(fromScreen(new Point(e.x, e.y)));
                this.dragging = true;
            }
        }

        prevY = e.y;
    }

    @Override
    public void mouseScrolled(MouseEvent e) {
        if (provider == null)
            return;

        dolly(e.count * 0.01);
        redraw();
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        if (this.presenter == null)
            return;

        // Check for single selection
        Map<Integer, List<Integer>> points = getSelectedPoints();
        if (points.size() == 1) {
            for (Map.Entry<Integer, List<Integer>> entry : points.entrySet()) {
                if (entry.getValue().size() == 1) {
                    this.presenter.onRemove();
                    return;
                }
            }
        }
        this.presenter.onAddPoint(fromScreen(new Point(e.x, e.y)));
    }

    @Override
    public void mouseDown(MouseEvent e) {
        cameraMovement = CameraUtil.getMovement(e);
        if (cameraMovement == Movement.IDLE) {
            if (SceneUtil.showContextMenu(e)) {
                this.contextMenu.setVisible(true);
            } else if (e.button == 1) {
                this.dragging = true;
                this.presenter.onStartDrag(
                        fromScreen(new Point(e.x, e.y)),
                        new Vector2d(getPlotWidth(), -zoomY),
                        SCREEN_DRAG_PADDING,
                        SCREEN_HIT_PADDING,
                        SCREEN_TANGENT_LENGTH);
            }
        }
        prevY = e.y;
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (this.dragging && e.button == 1) {
            this.presenter.onEndDrag();
        }

        cameraMovement = Movement.IDLE;
    }

    /**
     * Zoom to fit
     * @param zoomScale extra margin, e.g. 1.1 yields 10% extra
     */
    public void fit(double zoomScale) {
        double[] e = new double[2];
        double min = Double.MAX_VALUE;
        double max = -Double.MAX_VALUE;

        Object[] elements = this.contentProvider.getElements(this.input);
        for (int i = 0; i < elements.length; ++i) {
            if (!provider.isEnabled(i))
                continue;
            HermiteSpline spline = this.provider.getSpline(i);
            spline.getExtremValues(e);
            min = Math.min(min, e[0]);
            max = Math.max(max, e[1]);
        }

        if (min != Double.MAX_VALUE) {
            // We must have at least one spline

            double dist = max - min;
            min -= (dist * zoomScale - dist) / 2.0;
            max += (dist * zoomScale - dist) / 2.0;

            Point size = getSize();
            double plotHeight = size.y;

            /*
             * The transformation to screen space
             * yScreen = plotHeight - (y * zoomY - offsetY)
             *
             * Solve the equations:
             *
             *   0 = plotHeight - (max * zoomY - offsetY)           (1)
             *   plotHeight = plotHeight - (min * zoomY - offsetY)  (2)
             *
             * <=>
             *
             *   0 = plotHeight - (max * zoomY - offsetY)           (3)
             *   0 = -(min * zoomY - offsetY)                       (4)
             *
             * <=>
             *
             *   0 = plotHeight - (max * zoomY - offsetY)           (5)
             *   offsetY = min * zoomY                              (6)
             *
             *   (5) + (6)
             *
             * =>
             *
             *   0 = plotHeight - (max * zoomY - min * zoomY) = plotHeight - zoomY * (max - min)
             *
             * <=>
             *
             *   zoomY = plotHeight / (max - min)                   (7)
             *
             *
             */

            if (Math.abs(max - min) <= 0.001) {
                min -= 5;
                max += 5;
            }
            zoomY = plotHeight / (max - min);

            offsetY = min * zoomY;
        }

    }

    @Override
    public ISelection getSelection() {
        return this.selection;
    }

    @Override
    public void addSelectionChangedListener(ISelectionChangedListener listener) {
        // TODO Auto-generated method stub

    }

    @Override
    public void removeSelectionChangedListener(ISelectionChangedListener listener) {
        // TODO Auto-generated method stub

    }

}
