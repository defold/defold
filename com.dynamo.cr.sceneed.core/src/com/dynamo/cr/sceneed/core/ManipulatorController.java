package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import javax.annotation.PreDestroy;
import javax.inject.Inject;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.ISelectionService;
import org.eclipse.ui.IWorkbenchPart;

import com.dynamo.cr.editor.core.ILogger;

public class ManipulatorController implements ISelectionListener, IRenderViewProvider, MouseListener, MouseMoveListener {

    private ISelectionService selectionService;
    private IRenderView renderView;
    private IManipulatorRegistry manipulatorRegistry;
    private IManipulatorMode manipulatorMode;
    private List<Node> selectionList = new ArrayList<Node>();
    private Manipulator manipulator;
    private IOperationHistory undoHistory;
    private IUndoContext undoContext;
    private ILogger logger;

    @Inject
    public ManipulatorController(ISelectionService selectionService,
                                 IRenderView renderView,
                                 IManipulatorRegistry manipulatorRegistry,
                                 IOperationHistory undoHistory,
                                 IUndoContext undoContext,
                                 ILogger logger) {
        this.selectionService = selectionService;
        this.renderView = renderView;
        this.manipulatorRegistry = manipulatorRegistry;
        this.undoHistory = undoHistory;
        this.undoContext = undoContext;
        this.logger = logger;
        this.renderView.addMouseListener(this);
        this.renderView.addMouseMoveListener(this);
        this.renderView.addRenderProvider(this);
        this.selectionService.addSelectionListener(this);
    }

    public void setSelectionService(ISelectionService selectionService) {
        this.selectionService = selectionService;
    }

    public void setManipulatorMode(IManipulatorMode manipulatorMode) {
        this.manipulatorMode = manipulatorMode;
    }

    @PreDestroy
    public void dispose() {
        this.selectionService.removeSelectionListener(this);
        this.renderView.removeMouseListener(this);
        this.renderView.removeMouseMoveListener(this);
        this.renderView.removeRenderProvider(this);
    }

    @Override
    public void onNodeHit(List<Node> nodes) {
    }

    private void selectManipulator() {
        if (selectionList.size() > 0 && manipulatorMode != null) {
            Object[] selection = selectionList.toArray();
            manipulator = manipulatorRegistry.getManipulatorForSelection(manipulatorMode, selection);
            manipulator.setController(this);
        } else {
            manipulator = null;
        }
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        this.selectionList.clear();
        if (selection instanceof IStructuredSelection) {
            IStructuredSelection structSel = (IStructuredSelection) selection;
            Iterator<?> i = structSel.iterator();
            while (i.hasNext()) {
                Object o = i.next();
                if (o instanceof Node) {
                    Node node = (Node) o;
                    this.selectionList.add(node);
                }
            }
        }
        selectManipulator();
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        if (manipulator != null) {
            manipulator.mouseDoubleClick(e);
        }
    }

    @Override
    public void mouseDown(MouseEvent e) {
        if (manipulator != null) {
            manipulator.mouseDown(e);
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (manipulator != null) {
            manipulator.mouseUp(e);
        }
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (manipulator != null) {
            manipulator.mouseMove(e);
        }
    }

    public void executeOperation(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.undoHistory.execute(operation, null, null);
        } catch (final ExecutionException e) {
            this.logger.logException(e);
        }

        if (status != Status.OK_STATUS) {
            this.logger.logException(status.getException());
        }

    }

    @Override
    public void setup(RenderContext renderContext) {
        // TODO Auto-generated method stub

    }

}
