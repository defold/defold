package com.dynamo.cr.parted.curve;

import org.eclipse.jface.resource.ColorRegistry;
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
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Composite;

/**
 * Curve Editor
 * @author chmu
 */

public class CurveEditor extends Canvas implements PaintListener,
        MouseWheelListener, MouseMoveListener, MouseListener {

    public static final String BACKGROUND_COLOR_KEY = "com.dynamo.cr.parted.curve.BACKGROUND_COLOR";
    public static final String AXIS_COLOR_KEY = "com.dynamo.cr.parted.curve.AXIS_COLOR";
    public static final String GRID_COLOR_KEY = "com.dynamo.cr.parted.curve.GRID_COLOR";
    public static final String TICK_COLOR_KEY = "com.dynamo.cr.parted.curve.TICK_COLOR";
    public static final String TANGENT_COLOR_KEY = "com.dynamo.cr.parted.curve.TANGENT_COLOR";
    public static final String CONTROL_COLOR_KEY = "com.dynamo.cr.parted.curve.CONTROL_COLOR";
    public static final String SPLINE_COLOR_KEY = "com.dynamo.cr.parted.curve.SPLINE_COLOR";

    private int yAxisWidth = 60;
    private int marginRight = 32;
    private int tickHeight = 12;
    private int controlSize = 6;
    private int tangentControlSize = 6;
    private int subDivisions = 55;

    private double offsetY = 0;
    private double zoomY = 100;
    private int prevY = -1;
    private Mode mode = Mode.IDLE;
    private Hit hit;
    private Font font;

    private ICurveProvider provider;
    private Object[] input = new Object[0];

    private int activeIndex = -1;
    private HermiteSpline activeSpline = null;

    private ColorRegistry colorsRegistry;

    private enum Mode {
        IDLE, PANNING, MOVE,
    }

    private Color getColor(String key) {
        return colorsRegistry.get(key);
    }

    public CurveEditor(Composite parent, int style, ColorRegistry colorRegistry) {
        super(parent, style);
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

    public void setInput(Object[] input) {
        this.input = input;
    }

    public void setProvider(ICurveProvider provider) {
        this.provider = provider;
    }

    private void putColor(String key, RGB rgb) {
        if (colorsRegistry.get(key) == null) {
            colorsRegistry.put(key, rgb);
        }
    }

    private void initColors() {
        putColor(BACKGROUND_COLOR_KEY, new RGB(255, 255, 255));
        putColor(AXIS_COLOR_KEY, new RGB(100, 100, 100));
        putColor(GRID_COLOR_KEY, new RGB(220, 220, 220));
        putColor(TICK_COLOR_KEY, new RGB(100, 100, 100));
        putColor(TANGENT_COLOR_KEY, new RGB(0, 0x88, 0xcc));
        putColor(CONTROL_COLOR_KEY, new RGB(0xff, 0x24, 0x1e));
        putColor(SPLINE_COLOR_KEY, new RGB(0xcc, 0x00, 0x2f));
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

    private void splineChanged(String label, int index, HermiteSpline oldSpline, HermiteSpline newSpline) {
        provider.setSpline(newSpline, index);
    }

    double toScreenX(double xValue) {
        Point size = getSize();
        int plotWidth = size.x - yAxisWidth - marginRight;
        return (xValue * plotWidth + yAxisWidth);
    }

    double toScreenY(double yValue) {
        Point size = getSize();
        int plotHeight = size.y;
        double y = (yValue) * zoomY - offsetY;
        return (plotHeight - y);
    }

    double fromScreenX(int xValue) {
        Point size = getSize();
        int plotWidth = size.x - yAxisWidth - marginRight;
        double x = (xValue - yAxisWidth) / (double) plotWidth;
        return x;
    }

    double fromScreenY(int yValue) {
        Point size = getSize();
        int plotHeight = size.y;
        yValue = plotHeight - yValue;
        double y = (yValue + offsetY) / zoomY;
        return y;
    }

    private double getTangentScale(SplinePoint p) {
        double tangentLength = 110.0;
        double a = toScreenX(p.getTx()) - toScreenX(0);
        double b = toScreenY(p.getTy()) - toScreenY(0);
        double unitLength = 2 * Math.sqrt(a * a + b * b);

        double scale = tangentLength / unitLength;
        return scale;
    }

    private void drawGrid(GC gc) {
        Point size = getSize();

        int plotWidth = size.x - yAxisWidth - marginRight;

        for (int i = 0; i < 10 + 1; i++) {
            String s = String.format("%d", i * 10);
            Point extents = gc.stringExtent(s);

            gc.setForeground(getColor(GRID_COLOR_KEY));
            int x = yAxisWidth + i * plotWidth / 10;
            int y = (int) toScreenY(0) + tickHeight / 2 + 2; // + 2 for some
                                                             // extra margin
            gc.drawLine(x, 0, x, size.y);

            gc.setForeground(getColor(TICK_COLOR_KEY));
            gc.drawString(s, x - extents.x / 2, y);
            int tickX = yAxisWidth + i * plotWidth / 10;
            int tickY = (int) toScreenY(0);
            gc.drawLine(tickX, tickY - tickHeight / 2, tickX, tickY
                    + tickHeight / 2);
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
            int x = yAxisWidth;
            int y = (int) toScreenY(valueY);

            gc.setForeground(getColor(GRID_COLOR_KEY));
            gc.drawLine(yAxisWidth, y, size.x, y);

            gc.setForeground(getColor(TICK_COLOR_KEY));
            gc.drawString(s, yAxisWidth - extents.x - 10, y - extents.y / 2);
            gc.drawLine(x - tickHeight / 2, y, x + tickHeight / 2, y);
            valueY += yStep;
        }
    }

    private void drawSpline(GC gc, HermiteSpline spline, Color splineColor) {
        int segCount = spline.getSegmentCount();
        double[] value = new double[2];
        double[] prevValue = new double[2];
        gc.setForeground(splineColor);

        int[] points = new int[subDivisions * 2];
        for (int s = 0; s < segCount; s++) {
            spline.getValue(s, 0, prevValue);
            points[0] = (int) toScreenX(prevValue[0]);
            points[1] = (int) toScreenY(prevValue[1]);
            for (int i = 1; i < subDivisions; ++i) {
                double t = i / (double) (subDivisions - 1);
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

            int x = (int) toScreenX(p.getX());
            int y = (int) toScreenY(p.getY());
            double scale = getTangentScale(p);
            int x0 = (int) toScreenX(p.getX() - p.getTx() * scale);
            int y0 = (int) toScreenY(p.getY() - p.getTy() * scale);
            int x1 = (int) toScreenX(p.getX() + p.getTx() * scale);
            int y1 = (int) toScreenY(p.getY() + p.getTy() * scale);

            gc.setForeground(getColor(TANGENT_COLOR_KEY));
            gc.setBackground(getColor(TANGENT_COLOR_KEY));
            gc.drawLine(x0, y0, x1, y1);

            gc.fillOval(x0 - tangentControlSize / 2, y0 - tangentControlSize
                    / 2, tangentControlSize, tangentControlSize);

            gc.fillOval(x1 - tangentControlSize / 2, y1 - tangentControlSize
                    / 2, tangentControlSize, tangentControlSize);

            gc.setForeground(getColor(CONTROL_COLOR_KEY));
            gc.setBackground(getColor(BACKGROUND_COLOR_KEY));
            gc.fillOval(x - controlSize / 2, y - controlSize / 2, controlSize,
                    controlSize);
            gc.drawOval(x - controlSize / 2, y - controlSize / 2, controlSize,
                    controlSize);

        }
    }

    @Override
    public void paintControl(PaintEvent e) {
        if (!isEnabled())
            return;

        GC gc = e.gc;
        drawGrid(gc);
        for (int i = 0; i < input.length; ++i) {
            if (!provider.isEnabled(i))
                continue;

            HermiteSpline spline = this.provider.getSpline(i);
            Color color = this.provider.getColor(i);
            if (color == null) {
                color = getColor(SPLINE_COLOR_KEY);
            }
            if (i == activeIndex) {
                spline = activeSpline;
            }
            drawSpline(gc, spline, color);
        }
    }

    boolean hit(MouseEvent e, int x, int y) {
        int hitRadius = 16;
        int dx = (x - e.x);
        int dy = (y - e.y);
        double dist = Math.sqrt(dx * dx + dy * dy);
        return dist < hitRadius;
    }

    private static enum Handle {
        CONTROL, TANGENT_CONTROL1, TANGENT_CONTROL2;
    }

    private static class Hit {
        private int index;
        private Handle handle;
        private SplinePoint point;
        private int mouseDX;
        private int mouseDY;

        Hit(int index, Handle handle, SplinePoint point, int dx, int dy) {
            this.index = index;
            this.handle = handle;
            this.point = point;
            this.mouseDX = dx;
            this.mouseDY = dy;
        }

    }

    private Hit hitTest(MouseEvent e, HermiteSpline spline) {
        int nPoints = spline.getCount();
        for (int i = 0; i < nPoints; ++i) {
            SplinePoint p = spline.getPoint(i);

            int x = (int) toScreenX(p.getX());
            int y = (int) toScreenY(p.getY());
            double scale = getTangentScale(p);
            int x0 = (int) toScreenX(p.getX() - p.getTx() * scale);
            int y0 = (int) toScreenY(p.getY() - p.getTy() * scale);
            int x1 = (int) toScreenX(p.getX() + p.getTx() * scale);
            int y1 = (int) toScreenY(p.getY() + p.getTy() * scale);

            if (hit(e, x, y)) {
                return new Hit(i, Handle.CONTROL, p, e.x - x, e.y - y);
            } else if (hit(e, x0, y0)) {
                return new Hit(i, Handle.TANGENT_CONTROL1, p, e.x - x0, e.y - y0);
            } else if (hit(e, x1, y1)) {
                return new Hit(i, Handle.TANGENT_CONTROL2, p, e.x - x1, e.y - y1);
            }
        }

        return null;
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (provider == null)
            return;

        int dy = e.y - prevY;
        if (mode == Mode.PANNING) {
            offsetY += dy;
            redraw();
        } else if (mode == Mode.MOVE && activeSpline != null) {

            if (hit.handle == Handle.CONTROL) {
                double newX = fromScreenX(e.x - hit.mouseDX) ;
                double newY = fromScreenY(e.y - hit.mouseDY);
                activeSpline = activeSpline.setPosition(hit.index, newX, newY);
                redraw();
            } else if (hit.handle == Handle.TANGENT_CONTROL1
                    || hit.handle == Handle.TANGENT_CONTROL2) {
                double tx = hit.point.getX() - fromScreenX(e.x - hit.mouseDX);
                double ty = hit.point.getY() - fromScreenY(e.y - hit.mouseDY);
                if (hit.handle == Handle.TANGENT_CONTROL2) {
                    tx *= -1;
                    ty *= -1;
                }

                double a = Math.atan2(ty, tx);
                if (Math.abs(a) > HermiteSpline.MAX_TANGENT_ANGLE) {
                    a = Math.signum(a) * HermiteSpline.MAX_TANGENT_ANGLE;
                    tx = Math.cos(a);
                    ty = Math.sin(a);
                }
                activeSpline = activeSpline.setTangent(hit.index, tx, ty);
                redraw();
            }
        }

        prevY = e.y;
    }

    @Override
    public void mouseScrolled(MouseEvent e) {
        if (provider == null)
            return;

        Point size = getSize();

        double center = (fromScreenY(0) + fromScreenY(size.y)) * 0.5;
        double screenCenter0 = toScreenY(center);

        zoomY += 0.01 * e.count * zoomY;
        zoomY = Math.max(0.01, zoomY);
        double screenCenter1 = toScreenY(center);
        offsetY += (screenCenter0 - screenCenter1);

        redraw();
    }

    private int closestSpline(MouseEvent e) {
        int index = -1;
        double distance = Double.MAX_VALUE;
        for (int i = 0; i < input.length; ++i) {
            if (!provider.isEnabled(i))
                continue;

            HermiteSpline spline = provider.getSpline(i);

            double d = Math.abs(spline.getY(fromScreenX(e.x)) - fromScreenY(e.y));
            if (d < distance) {
                distance = d;
                index = i;
            }
        }
        return index;
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        if (provider == null)
            return;

        activeIndex = -1;
        activeSpline = null;

        int index = closestSpline(e);
        if (index == -1) {
            return;
        }

        HermiteSpline spline = provider.getSpline(index);
        mode = Mode.IDLE;
        double x = fromScreenX(e.x);
        double y = spline.getY(x);

        e.x = (int) toScreenX(x);
        e.y = (int) toScreenY(y);

        Hit hit = hitTest(e, spline);
        if (hit != null && hit.handle == Handle.CONTROL) {
            splineChanged("Remove point", index, spline, spline.removePoint(hit.index));
        } else {
            splineChanged("Insert point", index, spline, spline.insertPoint(x, y));
        }
        redraw();
    }

    @Override
    public void mouseDown(MouseEvent e) {
        activeIndex = -1;
        activeSpline = null;

        for (int i = 0; i < input.length; ++i) {
            if (!provider.isEnabled(i))
                continue;

            HermiteSpline spline = provider.getSpline(i);
            hit = hitTest(e, spline);
            if (hit != null) {
                mode = Mode.MOVE;
                activeIndex = i;
                activeSpline = spline;
                return;
            }
        }
        mode = Mode.PANNING;
        prevY = e.y;
    }

    @Override
    public void mouseUp(MouseEvent e) {
        int i = activeIndex;
        HermiteSpline s = activeSpline;

        activeIndex = -1;
        activeSpline = null;

        if (mode == Mode.MOVE) {
            if (hit.handle == Handle.CONTROL) {
                HermiteSpline oldSpline = s.setPosition(hit.index, hit.point.getX(), hit.point.getY());
                splineChanged("Move point", i, oldSpline, s);
                redraw();
            } else if (hit.handle == Handle.TANGENT_CONTROL1
                    || hit.handle == Handle.TANGENT_CONTROL2) {

                HermiteSpline oldSpline = s.setTangent(hit.index, hit.point.getTx(), hit.point.getTy());
                splineChanged("Set tangent", i, oldSpline, s);
                redraw();
            }
        }

        mode = Mode.IDLE;
    }

    /**
     * Zoom to fit
     * @param zoomScale extra margin, e.g. 1.1 yields 10% extra
     */
    public void fit(double zoomScale) {
        double[] e = new double[2];
        double min = Double.MAX_VALUE;
        double max = -Double.MAX_VALUE;

        for (int i = 0; i < input.length; ++i) {
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

}
