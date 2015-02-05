package internal.ui;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Event;
import org.eclipse.ui.forms.widgets.FormToolkit;

import clojure.lang.IFn;

public class ResourceSelector extends Composite {
    private final IFn selectionCallback;
    private final Button button;
    private final SelectionListener listener;

    public ResourceSelector(FormToolkit toolkit, Composite parent, int style, IFn selectionCallback) {
        super(parent, SWT.NONE);
        setLayout(new FillLayout());
        toolkit.adapt(this);

        this.selectionCallback = selectionCallback;

        button = toolkit.createButton(this, "...", style);

        listener = new SelectionListener() {
            public void widgetSelected(SelectionEvent e) {
                Object selection = ResourceSelector.this.selectionCallback.invoke();
                if (selection != null) {
                    Event event = new Event();
                    event.data = selection;
                    notifyListeners(SWT.Selection, event);
                }
            }

            public void widgetDefaultSelected(SelectionEvent e) {
                // From Button javadoc: widgetDefaultSelected is not called.
            }
        };

        button.addSelectionListener(listener);

        addDisposeListener(new DisposeListener() {
            public void widgetDisposed(DisposeEvent e) {
                button.removeSelectionListener(listener);
            }
        });
    }

    public Button getButton() {
        return button;
    }
}
