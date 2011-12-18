package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.Collections;
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
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.ISelectionService;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.part.IContributedContentsView;
import org.eclipse.ui.views.contentoutline.ContentOutline;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.sceneed.ui.RootManipulator;

public class ManipulatorController implements ISelectionListener, IRenderViewProvider, MouseListener, MouseMoveListener {

    private ISelectionService selectionService;
    private IRenderView renderView;
    private IManipulatorRegistry manipulatorRegistry;
    private IManipulatorMode manipulatorMode;
    private List<Node> selectionList = new ArrayList<Node>();
    private RootManipulator rootManipulator;
    private Manipulator selectedManipulator;
    private IOperationHistory undoHistory;
    private IUndoContext undoContext;
    private ILogger logger;
    private IEditorPart editorPart;

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

    public IRenderView getRenderView() {
        return renderView;
    }

    public void setSelectionService(ISelectionService selectionService) {
        this.selectionService = selectionService;
    }

    public void setManipulatorMode(IManipulatorMode manipulatorMode) {
        this.manipulatorMode = manipulatorMode;
        selectManipulator();
        this.renderView.refresh();
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
        this.selectedManipulator = null;
        for (Node node : nodes) {
            if (node instanceof Manipulator) {
                Manipulator m = (Manipulator) node;
                this.selectedManipulator = m;
                return;
            }
        }
    }

    public RootManipulator getRootManipulator() {
        return rootManipulator;
    }

    private void selectManipulator() {
        if (selectionList.size() > 0 && manipulatorMode != null) {
            Object[] selection = selectionList.toArray();
            rootManipulator = manipulatorRegistry.getManipulatorForSelection(manipulatorMode, selection);
            if (rootManipulator != null) {
                rootManipulator.setController(this);
                rootManipulator.setSelection(Collections.unmodifiableList(selectionList));
            }
        } else {
            rootManipulator = null;
        }
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        boolean currentSelection = false;
        if (part == editorPart) {
            currentSelection = true;
        } else if (part instanceof ContentOutline) {
            IContributedContentsView view = (IContributedContentsView)((ContentOutline)part).getAdapter(IContributedContentsView.class);
            currentSelection = view.getContributingPart() == editorPart;
        }

        if (currentSelection) {
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
    }

    private static List<Manipulator> getAllManipulators(Manipulator m) {
        List<Manipulator> lst = new ArrayList<Manipulator>(16);
        doGetAllManipulators(m, lst);
        return lst;
    }

    private static void doGetAllManipulators(Manipulator m, List<Manipulator> lst) {
        if (m == null)
            return;

        lst.add(m);

        for (Node n : m.getChildren()) {
            Manipulator c = (Manipulator) n;
            doGetAllManipulators(c, lst);
        }
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        if (CameraController.hasCameraControlModifiers(e)) return;

        List<Manipulator> lst = getAllManipulators(this.rootManipulator);
        for (Manipulator m : lst) {
            m.mouseDoubleClick(e);
        }
    }

    @Override
    public void mouseDown(MouseEvent e) {
        if (CameraController.hasCameraControlModifiers(e)) return;

        List<Manipulator> lst = getAllManipulators(this.rootManipulator);
        for (Manipulator m : lst) {
            m.mouseDown(e);
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (CameraController.hasCameraControlModifiers(e)) return;

        List<Manipulator> lst = getAllManipulators(this.rootManipulator);
        for (Manipulator m : lst) {
            m.mouseUp(e);
        }
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (CameraController.hasCameraControlModifiers(e)) return;

        List<Manipulator> lst = getAllManipulators(this.rootManipulator);
        for (Manipulator m : lst) {
            m.mouseMove(e);
        }
        this.renderView.refresh();
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
        if (this.rootManipulator != null) {
            renderView.setupNode(renderContext, this.rootManipulator);
        }
    }

    public boolean isManipulatorSelected(Manipulator m) {
        return m == this.selectedManipulator;
    }

    public void setEditorPart(IEditorPart editorPart) {
        this.editorPart = editorPart;
    }

    public void refresh() {
        if (rootManipulator != null) {
            rootManipulator.refresh();
        }
    }

}
