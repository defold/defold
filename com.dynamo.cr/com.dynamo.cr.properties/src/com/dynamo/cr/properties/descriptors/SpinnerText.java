package com.dynamo.cr.properties.descriptors;

import java.text.DecimalFormat;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Text;

/**
 * Spinner like control
 * @author chmu
 *
 */
public class SpinnerText extends Composite implements MouseListener, MouseMoveListener {

    private Text text;
    private Point startCursorLocation;
    private int startDragX = -1;
    private Double currentValue = null;
    private boolean integer = false;
    private DecimalFormat doubleFormat = new DecimalFormat("#.####");
    private DecimalFormat integerFormat = new DecimalFormat("#");
    private double min = -Double.MAX_VALUE;
    private double max = Double.MAX_VALUE;

    public SpinnerText(Composite parent, int style, boolean integer) {
        super(parent, SWT.NONE);
        this.integer = integer;
        setLayout(new FillLayout());
        text = new Text(this, style);
        text.addMouseListener(this);
        text.addMouseMoveListener(this);
    }

    @Override
    public void dispose() {
        text.removeMouseListener(this);
        text.removeMouseMoveListener(this);
    }

    public void setMin(double min) {
        this.min = min;
    }

    public void setMax(double max) {
        this.max = max;
    }

    public Text getText() {
        return text;
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (startDragX != -1) {
            Display.getCurrent().setCursorLocation(startCursorLocation);

            int dx = e.x - startDragX;

            if (currentValue != null) {
                double step = 0.01;
                if (Math.abs(dx) > 10) {
                    step = 0.1;
                } else if (Math.abs(dx) > 100) {
                    step = 1.0;
                }

                DecimalFormat format;
                if (integer) {
                    format = integerFormat;
                    step *= 10;
                } else {
                    format = doubleFormat;
                }

                currentValue += dx * step;
                currentValue = Math.max(currentValue, min);
                currentValue = Math.min(currentValue, max);
                String stringValue = format.format(currentValue);
                text.setText(stringValue);
                sendSelectionEvent(true);
            }
        }
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {}

    @Override
    public void mouseDown(MouseEvent e) {
        if ((text.getStyle() & SWT.READ_ONLY) == 0 && (e.stateMask & (SWT.MOD1 | SWT.MOD2)) != 0) {
            currentValue = null;
            try {
                currentValue = Double.parseDouble(text.getText());
            } catch (NumberFormatException ex) {}
            startCursorLocation = Display.getCurrent().getCursorLocation();
            startDragX = e.x;
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (startDragX != -1) {
            sendSelectionEvent(false);
        }
        startDragX = -1;
        this.setFocus();
    }

    private void sendSelectionEvent(boolean drag) {
        Display display = Display.getCurrent();
        Event event = new Event ();
        event.type = SWT.DefaultSelection;
        event.display = display;
        event.widget = text;
        // By convention SWT.DRAG is interpreted as "live" editing
        // the spinner. Upon release SWT.DRAG is not set and should be
        // interpreted as a commit operation
        event.detail = drag ? SWT.DRAG : 0;
        text.notifyListeners(SWT.DefaultSelection, event);
    }

}
