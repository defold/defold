package com.dynamo.cr.parted;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.resource.JFaceResources;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.ListViewer;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.IWorkbenchPartSite;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.parted.curve.CurveEditor;
import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.parted.curve.ICurveEditorListener;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;

public class ParticleFXCurveEditorPage implements ICurveEditorPage, ISelectionListener, ISelectionChangedListener, IOperationHistoryListener, ICurveEditorListener {

    private static Logger logger = LoggerFactory
            .getLogger(ParticleFXCurveEditorPage.class);
    private CurveEditor curveEditor;
    private IWorkbenchPartSite site;
    private IUndoContext undoContext;
    private Node selectedNode;
    private Composite composite;
    private ListViewer list;
    private IOperationHistory history;
    private List<IPropertyDesc<Node, IPropertyObjectWorld>> oldInput = new ArrayList<IPropertyDesc<Node,IPropertyObjectWorld>>();
    private UndoActionHandler undoHandler;
    private RedoActionHandler redoHandler;

    public ParticleFXCurveEditorPage(UndoActionHandler undoHandler, RedoActionHandler redoHandler) {
        this.undoHandler = undoHandler;
        this.redoHandler = redoHandler;
    }

    @Override
    public void createControl(Composite parent) {
        composite = new Composite(parent, SWT.NONE);
        GridLayout layout = new GridLayout(2, false);
        layout.verticalSpacing = 0;
        layout.marginWidth = layout.marginHeight = 0;
        composite.setLayout(layout);

        list = new ListViewer(composite, SWT.SINGLE);
        GridData gd = new GridData(GridData.FILL_VERTICAL);
        gd.widthHint = 120;
        list.getControl().setLayoutData(gd);
        list.setContentProvider(new ArrayContentProvider());
        list.setLabelProvider(new LabelProvider() {
           @Override
           public String getText(Object element) {
               if (element instanceof IPropertyDesc<?, ?>) {
                   IPropertyDesc<?, ?> pd = (IPropertyDesc<?, ?>) element;
                   return pd.getName();
               }
               return super.getText(element);
           }
        });
        list.addSelectionChangedListener(this);

        history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        history.addOperationHistoryListener(this);
        curveEditor = new CurveEditor(composite, SWT.NONE, this, JFaceResources.getColorRegistry());
        curveEditor.setLayoutData(new GridData(GridData.FILL_BOTH));
    }

    @Override
    public void init(IViewSite site) {
        this.site = site;
        site.getPage().addSelectionListener(this);
        IActionBars actionBars = site.getActionBars();
        actionBars.setGlobalActionHandler(ActionFactory.UNDO.getId(), this.undoHandler);
        actionBars.setGlobalActionHandler(ActionFactory.REDO.getId(), this.redoHandler);
    }

    @Override
    public void dispose() {
        site.getPage().removeSelectionListener(this);
        if (history != null) {
            history.removeOperationHistoryListener(this);
        }
    }

    @Override
    public Control getControl() {
        return composite;
    }

    @Override
    public void setActionBars(IActionBars actionBars) {
    }

    @Override
    public void setFocus() {
        composite.setFocus();
    }

    @SuppressWarnings({ "unchecked" })
    public void refresh() {
        List<IPropertyDesc<Node, IPropertyObjectWorld>> input = new ArrayList<IPropertyDesc<Node,IPropertyObjectWorld>>();
        if (selectedNode != null) {
            IPropertyModel<Node, IPropertyObjectWorld> propertyModel = (IPropertyModel<Node, IPropertyObjectWorld>) selectedNode.getAdapter(IPropertyModel.class);
            IPropertyDesc<Node, IPropertyObjectWorld>[] descs = propertyModel.getPropertyDescs();
            for (IPropertyDesc<Node, IPropertyObjectWorld> pd : descs) {
                Object value = propertyModel.getPropertyValue(pd.getId());
                if (value instanceof ValueSpread) {
                    ValueSpread vs = (ValueSpread) value;
                    if (vs.isAnimated()) {
                        ((HermiteSpline)vs.getCurve()).setUserdata(pd);
                        input.add(pd);
                    }
                }
            }
        }

        if (!input.equals(oldInput)) {
            list.setInput(input.toArray());
            curveEditor.setSpline(null);
        }

        if (curveEditor.getSpline() != null) {
            // Re-read spline as the spline is immutable
            IPropertyDesc<Node, IPropertyObjectWorld> pd = (IPropertyDesc<Node, IPropertyObjectWorld>) curveEditor.getSpline().getUserData();
            IPropertyModel<Node, IPropertyObjectWorld> propertyModel = ((IPropertyModel<Node, IPropertyObjectWorld>) selectedNode.getAdapter(IPropertyModel.class));
            ValueSpread vs = (ValueSpread) propertyModel.getPropertyValue(pd.getId());
            curveEditor.setSpline((HermiteSpline) vs.getCurve());
        }

        oldInput = input;

        list.refresh();
        curveEditor.redraw();
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        Node newNode = null;

        String id = part.getSite().getId();
        if (!(id.equals("org.eclipse.ui.views.ContentOutline") || part instanceof EditorPart)) {
            // Filter out not interesting selections
            return;
        }

        if (selection instanceof IStructuredSelection && !selection.isEmpty()) {
            IStructuredSelection structSelect = (IStructuredSelection) selection;
            Object first = structSelect.getFirstElement();
            if (first instanceof Node) {
                newNode = (Node) first;
            }

            IEditorPart editor = site.getPage().getActiveEditor();
            undoContext = (IUndoContext) editor.getAdapter(IUndoContext.class);
        }

        if (newNode != null && undoContext != null) {
            curveEditor.setEnabled(true);
            selectedNode = newNode;
        } else {
            curveEditor.setEnabled(false);
            selectedNode = null;
        }

        refresh();
    }

    @SuppressWarnings("unchecked")
    @Override
    public void selectionChanged(SelectionChangedEvent event) {
        ISelection selection = event.getSelection();

        HermiteSpline oldSel = curveEditor.getSpline();
        HermiteSpline newSel = null;

        if (!selection.isEmpty()) {
            IPropertyDesc<Node, IPropertyObjectWorld> pd = (IPropertyDesc<Node, IPropertyObjectWorld>) ((IStructuredSelection) selection).getFirstElement();
            IPropertyModel<Node, IPropertyObjectWorld> propertyModel = ((IPropertyModel<Node, IPropertyObjectWorld>) selectedNode.getAdapter(IPropertyModel.class));
            newSel = (HermiteSpline) ((ValueSpread) propertyModel.getPropertyValue(pd.getId())).getCurve();
        } else {
            curveEditor.setSpline(null);
        }

        SelectSplineOperation op = new SelectSplineOperation("Select spline", curveEditor, oldSel, newSel);
        op.addContext(this.undoContext);
        try {
            history.execute(op, new NullProgressMonitor(), null);
        } catch (ExecutionException e) {
            logger.error("Failed to execute set-spline operation", e);
        }

    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        switch (event.getEventType()) {
        case OperationHistoryEvent.DONE:
        case OperationHistoryEvent.UNDONE:
        case OperationHistoryEvent.REDONE:
            if (event.getOperation().hasContext(undoContext)) {
                refresh();
            }
        }
    }

    @SuppressWarnings("unchecked")
    @Override
    public void splineChanged(String label, CurveEditor editor,
            HermiteSpline oldSpline, HermiteSpline newSpline) {

        IPropertyDesc<Node, IPropertyObjectWorld> pd = (IPropertyDesc<Node, IPropertyObjectWorld>) newSpline.getUserData();
        IPropertyModel<Node, IPropertyObjectWorld> propertyModel = (IPropertyModel<Node, IPropertyObjectWorld>) selectedNode.getAdapter(IPropertyModel.class);
        ValueSpread value = (ValueSpread) propertyModel.getPropertyValue(pd.getId());
        value.setCurve(newSpline);
        IUndoableOperation operation = propertyModel.setPropertyValue(pd.getId(), value);
        operation.addContext(undoContext);
        IStatus status = null;
        try {
            status = history.execute(operation, null, null);
            if (status != Status.OK_STATUS) {
                logger.error("Failed to execute operation", status.getException());
                throw new RuntimeException(status.toString());
            }
        } catch (final ExecutionException e) {
            logger.error("Failed to execute operation", e);
        }
    }
}

