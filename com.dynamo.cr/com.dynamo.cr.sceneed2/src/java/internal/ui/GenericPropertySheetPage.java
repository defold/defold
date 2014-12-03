package internal.ui;

import static clojure.osgi.ClojureHelper.*;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.views.properties.IPropertySheetPage;

public class GenericPropertySheetPage extends Viewer implements IPropertySheetPage {
    private static final String INTERNAL_NS = "internal.ui.property";

    static {
        require(INTERNAL_NS);
    }

    /**
     * This is a Clojure variable that provides the real implementation plugged
     * in to this generic property sheet.
     */
    private Object behavior;
    private Object scope;

    public GenericPropertySheetPage(Object scope) {
        super();

        this.scope = scope;
    }

    @Override
    public void createControl(Composite parent) {
        behavior = invoke(INTERNAL_NS, "implementation-for", scope);

        dispatchMessage(behavior, CREATE, PARENT, parent);
    }

    @Override
    public void dispose() {
        dispatchMessage(behavior, DESTROY);
    }

    @Override
    public void setActionBars(IActionBars actionBars) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setFocus() {
        dispatchMessage(behavior, FOCUS);
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        // TODO Auto-generated method stub

    }

    @Override
    public Control getControl() {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public Object getInput() {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public ISelection getSelection() {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public void refresh() {
        // TODO Auto-generated method stub

    }

    @Override
    public void setInput(Object input) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setSelection(ISelection selection, boolean reveal) {
        // TODO Auto-generated method stub

    }
}
