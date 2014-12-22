package internal.ui;

import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.forms.widgets.FormToolkit;

public class ColorSelector extends Composite {

    private final org.eclipse.jface.preference.ColorSelector myColorSelector;
    private final IPropertyChangeListener listener;

    public ColorSelector(FormToolkit toolkit, Composite parent, int style /* TODO: apply style to button */) {
        super(parent, SWT.NONE);
        setLayout(new FillLayout());
        toolkit.adapt(this);

        myColorSelector = new org.eclipse.jface.preference.ColorSelector(this);
        toolkit.adapt(myColorSelector.getButton(), true, true);

        listener = new IPropertyChangeListener() {
            public void propertyChange(PropertyChangeEvent event) {
                notifyListeners(SWT.Selection, null);
            }
        };

        myColorSelector.addListener(listener);

        addDisposeListener(new DisposeListener() {
            public void widgetDisposed(DisposeEvent e) {
                myColorSelector.removeListener(listener);
            }
        });
    }

    public void setColorValue(RGB rgb) {
        myColorSelector.setColorValue(rgb);
    }

    public RGB getColorValue() {
        return myColorSelector.getColorValue();
    }

    public Button getButton() {
        return myColorSelector.getButton();
    }
}
