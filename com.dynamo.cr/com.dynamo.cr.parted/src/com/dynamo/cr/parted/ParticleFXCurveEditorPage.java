package com.dynamo.cr.parted;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.jface.resource.JFaceResources;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
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
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;

public class ParticleFXCurveEditorPage implements ICurveEditorPage, ISelectionListener, ISelectionChangedListener, IOperationHistoryListener {

    private static Logger logger = LoggerFactory
            .getLogger(ParticleFXCurveEditorPage.class);
    private CurveEditor curveEditor;
    private IWorkbenchPartSite site;
    private IUndoContext undoContext;
    private Node selectedNode;
    private Composite composite;
    private ListViewer list;
    private IOperationHistory history;
    private List<CurveEntry> oldInput = new ArrayList<CurveEntry>();
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
        list.addSelectionChangedListener(this);

        history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        history.addOperationHistoryListener(this);
        curveEditor = new CurveEditor(composite, SWT.NONE, null, history, JFaceResources.getColorRegistry());
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

    @SuppressWarnings("rawtypes")
    private static class CurveEntry {

        private IPropertyDesc desc;
        private ValueSpread valueSpread;

        public CurveEntry(IPropertyDesc pd, ValueSpread vs) {
            this.desc = pd;
            this.valueSpread = vs;
        }

        @Override
        public String toString() {
            return desc.getName();
        }

        public HermiteSpline getCurve() {
            return (HermiteSpline) valueSpread.getCurve();
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof CurveEntry) {
                CurveEntry ce = (CurveEntry) obj;
                return desc == ce.desc;
            }
            return super.equals(obj);
        }
    }

    @SuppressWarnings("rawtypes")
    public void refresh() {
        List<CurveEntry> input = new ArrayList<CurveEntry>();
        if (selectedNode != null) {
            IPropertyModel propertyModel = (IPropertyModel) selectedNode.getAdapter(IPropertyModel.class);
            IPropertyDesc[] descs = propertyModel.getPropertyDescs();
            for (IPropertyDesc pd : descs) {
                Object value = propertyModel.getPropertyValue(pd.getId());
                if (value instanceof ValueSpread) {
                    ValueSpread vs = (ValueSpread) value;
                    if (vs.isAnimated()) {
                        input.add(new CurveEntry(pd, vs));
                    }
                }
            }
        }

        if (!input.equals(oldInput)) {
            list.setInput(input.toArray());
            curveEditor.setSpline(null);
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
            curveEditor.setUndoContext(undoContext);
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

    @Override
    public void selectionChanged(SelectionChangedEvent event) {
        ISelection selection = event.getSelection();

        HermiteSpline oldSel = curveEditor.getSpline();
        HermiteSpline newSel = null;

        if (!selection.isEmpty()) {
            CurveEntry ce = (CurveEntry) ((IStructuredSelection) selection).getFirstElement();
            newSel = ce.getCurve();
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
}
