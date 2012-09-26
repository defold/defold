package com.dynamo.cr.parted.curve;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
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
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Curve Editor
 * @author chmu
 */

public class CurveEditor extends Canvas implements PaintListener,
        MouseWheelListener, MouseMoveListener, MouseListener {

    private static Logger logger = LoggerFactory.getLogger(CurveEditor.class);

    public static final String BACKGROUND_COLOR_KEY = "com.dynamo.cr.parted.curve.BACKGROUND_COLOR";
    public static final String AXIS_COLOR_KEY = "com.dynamo.cr.parted.curve.AXIS_COLOR";
    public static final String GRID_COLOR_KEY = "com.dynamo.cr.parted.curve.GRID_COLOR";
    public static final String TICK_COLOR_KEY = "com.dynamo.cr.parted.curve.TICK_COLOR";
    public static final String TANGENT_COLOR_KEY = "com.dynamo.cr.parted.curve.TANGENT_COLOR";
    public static final String CONTROL_COLOR_KEY = "com.dynamo.cr.parted.curve.CONTROL_COLOR";

    private int yAxisWidth = 60;
    private int marginRight = 32;
    private int tickHeight = 16;
    private int controlSize = 6;
    private int tangentControlSize = 6;
    private int subDivisions = 55;

    private double offsetY = 0;
    private double zoomY = 1;
    private int prevY = -1;
    private Mode mode = Mode.IDLE;
    private Hit hit;
    private Font font;

    private HermiteSpline spline;

    private IUndoContext undoContext;
    private IOperationHistory history;

    private ColorRegistry colorsRegistry;

    private enum Mode {
        IDLE, PANNING, MOVE,
    }

    private Color getColor(String key) {
        return colorsRegistry.get(key);
    }

    public CurveEditor(Composite parent, int style, UndoContext undoContext,
            IOperationHistory history, ColorRegistry colorRegistry) {
        super(parent, style);
        addPaintListener(this);
        addMouseWheelListener(this);
        addMouseMoveListener(this);
        addMouseListener(this);
        Font currentFont = getFont();
        font = new Font(getDisplay(), currentFont.getFontData()[0].getName(),
                10, SWT.NORMAL);
        setFont(font);
        this.undoContext = undoContext;
        this.history = history;
        this.colorsRegistry = colorRegistry;
        initColors();

        setBackground(getColor(BACKGROUND_COLOR_KEY));
    }

    private void putColor(String key, RGB rgb) {
        if (colorsRegistry.get(key) == null) {
            colorsRegistry.put(key, rgb);
        }
    }

    private void initColors() {
        putColor(BACKGROUND_COLOR_KEY, new RGB(255, 255, 255));
        putColor(AXIS_COLOR_KEY, new RGB(100, 100, 100));
        putColor(GRID_COLOR_KEY, new RGB(169, 169, 169));
        putColor(TICK_COLOR_KEY, new RGB(0, 0, 0));
        putColor(TANGENT_COLOR_KEY, new RGB(0, 200, 255));
        putColor(CONTROL_COLOR_KEY, new RGB(255, 0, 160));
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

    public void executeOperation(IUndoableOperation operation) {
        if (undoContext == null) {
            return;
        }

        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.history.execute(operation, null, null);
            if (status != Status.OK_STATUS) {
                logger.error("Failed to execute operation",
                        status.getException());
            }
        } catch (final ExecutionException e) {
            logger.error("Failed to execute operation", e);
        }
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
        double tangentLength = 100.0;
        double a = toScreenX(p.tx) - toScreenX(0);
        double b = toScreenY(p.ty) - toScreenY(0);
        double unitLength = 2 * Math.sqrt(a * a + b * b);

        double scale = tangentLength / unitLength;
        return scale;
    }

    @Override
    public void paintControl(PaintEvent e) {
        if (!isEnabled() || spline == null)
            return;

        GC gc = e.gc;
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

        int segCount = spline.getSegmentCount();
        double[] value = new double[2];
        double[] prevValue = new double[2];

        for (int s = 0; s < segCount; s++) {
            spline.getValue(s, 0, prevValue);

            for (int i = 1; i < subDivisions; ++i) {
                double t = i / (double) (subDivisions - 1);
                spline.getValue(s, t, value);

                gc.drawLine((int) toScreenX(prevValue[0]),
                        (int) toScreenY(prevValue[1]),
                        (int) toScreenX(value[0]), (int) toScreenY(value[1]));

                double[] tmp = prevValue;
                prevValue = value;
                value = tmp;
            }
        }

        int nPoints = spline.getCount();
        for (int i = 0; i < nPoints; ++i) {
            SplinePoint p = spline.getPoint(i);

            int x = (int) toScreenX(p.x);
            int y = (int) toScreenY(p.y);
            double scale = getTangentScale(p);
            int x0 = (int) toScreenX(p.x - p.tx * scale);
            int y0 = (int) toScreenY(p.y - p.ty * scale);
            int x1 = (int) toScreenX(p.x + p.tx * scale);
            int y1 = (int) toScreenY(p.y + p.ty * scale);

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

    private Hit hitTest(MouseEvent e) {
        int nPoints = spline.getCount();
        for (int i = 0; i < nPoints; ++i) {
            SplinePoint p = spline.getPoint(i);

            int x = (int) toScreenX(p.x);
            int y = (int) toScreenY(p.y);
            double scale = getTangentScale(p);
            int x0 = (int) toScreenX(p.x - p.tx * scale);
            int y0 = (int) toScreenY(p.y - p.ty * scale);
            int x1 = (int) toScreenX(p.x + p.tx * scale);
            int y1 = (int) toScreenY(p.y + p.ty * scale);

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
        if (spline == null)
            return;

        int dy = e.y - prevY;
        if (mode == Mode.PANNING) {
            offsetY += dy;
            redraw();
        } else if (mode == Mode.MOVE) {

            if (hit.handle == Handle.CONTROL) {
                double newX = fromScreenX(e.x - hit.mouseDX) ;
                double newY = fromScreenY(e.y - hit.mouseDY);
                spline.setPosition(hit.index, newX, newY);
                redraw();
            } else if (hit.handle == Handle.TANGENT_CONTROL1
                    || hit.handle == Handle.TANGENT_CONTROL2) {
                double tx = hit.point.x - fromScreenX(e.x - hit.mouseDX);
                double ty = hit.point.y - fromScreenY(e.y - hit.mouseDY);
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
                spline.setTangent(hit.index, tx, ty);
                redraw();
            }
        }

        prevY = e.y;
    }

    @Override
    public void mouseScrolled(MouseEvent e) {
        if (spline == null)
            return;

        zoomY += 0.01 * e.count * zoomY;
        zoomY = Math.max(0.01, zoomY);
        redraw();
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        if (spline == null)
            return;

        mode = Mode.IDLE;
        double x = fromScreenX(e.x);
        double y = spline.getY(x);

        e.x = (int) toScreenX(x);
        e.y = (int) toScreenY(y);

        Hit hit = hitTest(e);
        if (hit != null && hit.handle == Handle.CONTROL) {
            RemovePointOperation op = new RemovePointOperation("Insert point", spline, hit.index);
            executeOperation(op);
        } else {
            InsertPointOperation op = new InsertPointOperation("Insert point",
                    spline, x, y);
            executeOperation(op);
        }
        redraw();
    }

    @Override
    public void mouseDown(MouseEvent e) {
        if (spline == null)
            return;

        hit = hitTest(e);
        if (hit != null) {
            mode = Mode.MOVE;
        } else {
            mode = Mode.PANNING;
            prevY = e.y;
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (spline == null)
            return;

        if (mode == Mode.MOVE) {
            if (hit.handle == Handle.CONTROL) {
                SplinePoint p = spline.getPoint(hit.index);
                MovePointOperation op = new MovePointOperation("Move point", spline, hit.index, hit.point.x, hit.point.y, p.x, p.y);
                executeOperation(op);
                redraw();
            } else if (hit.handle == Handle.TANGENT_CONTROL1
                    || hit.handle == Handle.TANGENT_CONTROL2) {

                SplinePoint p = spline.getPoint(hit.index);
                MoveTangentOperation op = new MoveTangentOperation("Set tangent", spline, hit.index, hit.point.tx, hit.point.ty, p.tx, p.ty);
                executeOperation(op);

                redraw();
            }
        }

        mode = Mode.IDLE;
    }

    public void setSpline(HermiteSpline spline) {
        this.spline = spline;
    }

    public HermiteSpline getSpline() {
        return this.spline;
    }

    public void setUndoContext(IUndoContext undoContext) {
        this.undoContext = undoContext;
    }

}
