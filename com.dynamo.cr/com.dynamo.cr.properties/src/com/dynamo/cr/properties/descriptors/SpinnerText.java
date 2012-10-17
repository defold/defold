package com.dynamo.cr.properties.descriptors;

import java.text.DecimalFormat;

import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.ScrolledComposite;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseWheelListener;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Text;

/**
 * Spinner like control
 * @author chmu
 *
 */
public class SpinnerText extends Composite implements MouseWheelListener {

    private static boolean filterInstalled = false;

    private Text text;
    private boolean integer = false;
    private DecimalFormat doubleFormat = new DecimalFormat("#.####");
    private DecimalFormat integerFormat = new DecimalFormat("#");
    private double min = -Double.MAX_VALUE;
    private double max = Double.MAX_VALUE;

    /**
     * Global event filter. The filter consumes all MouseVerticalWheel if
     * MOD1 is pressed to {@link Text} widgets children of {@link SpinnerText}. Otherwise
     * the parent {@link ScrolledComposite} will get the events and scroll the properties view
     * TOOD: Better solution?
     */
    private static class Filter implements Listener {

        @Override
        public void handleEvent(Event event) {
            if (event.widget instanceof Control) {
                if (((Control) event.widget).getParent() instanceof SpinnerText) {
                    if ((event.stateMask & SWT.MOD1) != 0) {
                        event.doit = false;
                    }
                }
            }
        }
    }

    public SpinnerText(Composite parent, int style, boolean integer) {
        super(parent, SWT.NONE);
        installFilter();
        this.integer = integer;
        setLayout(new FillLayout());
        text = new Text(this, style);
        text.addMouseWheelListener(this);
    }

    private static void installFilter() {
        /*
         * Install *global* event filter.
         */

        if (filterInstalled)
            return;

        Filter filter = new Filter();
        Display.getDefault().addFilter(SWT.MouseVerticalWheel, filter);
        filterInstalled = true;
    }

    @Override
    public void dispose() {
        text.removeMouseWheelListener(this);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        text.setEnabled(enabled);
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

    private void sendSelectionEvent() {
        Display display = Display.getCurrent();
        Event event = new Event ();
        event.type = SWT.DefaultSelection;
        event.display = display;
        event.widget = text;
        text.notifyListeners(SWT.DefaultSelection, event);
    }

    @Override
    public void mouseScrolled(MouseEvent e) {
        if ((e.stateMask & SWT.MOD1) == 0) {
            return;
        }

        int dx = e.count;
        Double currentValue = null;
        try {
            currentValue = Double.parseDouble(text.getText());
        } catch (NumberFormatException ex) {}

        if (currentValue != null) {
            double step = 0.1;
            DecimalFormat format;
            if (integer) {
                format = integerFormat;
                step *= 10;
            } else {
                format = doubleFormat;
            }

            if ((e.stateMask & SWT.ALT) != 0) {
                step *= 10;
            }

            currentValue += dx * step;
            currentValue = Math.max(currentValue, min);
            currentValue = Math.min(currentValue, max);
            String stringValue = format.format(currentValue);
            text.setText(stringValue);
            sendSelectionEvent();
        }

    }
}
