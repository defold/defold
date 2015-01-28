package internal.ui;

import static clojure.osgi.ClojureHelper.*;

import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
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
    private final Object scope;
    private final UndoActionHandler undoHandler;
    private final RedoActionHandler redoHandler;

    public GenericPropertySheetPage(IEditorSite site, IUndoContext undoContext, Object scope) {
        super();

        this.scope = scope;

        undoHandler = new UndoActionHandler(site, undoContext);
        redoHandler = new RedoActionHandler(site, undoContext);
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
        final String undoId = ActionFactory.UNDO.getId();
        final String redoId = ActionFactory.REDO.getId();
        actionBars.setGlobalActionHandler(undoId, undoHandler);
        actionBars.setGlobalActionHandler(redoId, redoHandler);
    }

    @Override
    public void setFocus() {
        dispatchMessage(behavior, FOCUS);
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if (behavior != null) {
            ((ISelectionListener) behavior).selectionChanged(part, selection);
        }
    }

    @Override
    public Control getControl() {
        return (Control) invoke(INTERNAL_NS, "get-control", behavior);
    }

    @Override
    public Object getInput() {
        return null;
    }

    @Override
    public ISelection getSelection() {
        System.out.println("**** TODO: GenericPropertySheetPage.getSelection ****");
        return null;
    }

    @Override
    public void refresh() {
        System.out.println("**** TODO: GenericPropertySheetPage.refresh ****");
    }

    @Override
    public void setInput(Object input) {
        System.out.println("**** TODO: GenericPropertySheetPage.setInput ****");
    }

    @Override
    public void setSelection(ISelection selection, boolean reveal) {
        System.out.println("**** TODO: GenericPropertySheetPage.setSelection ****");
    }
}
