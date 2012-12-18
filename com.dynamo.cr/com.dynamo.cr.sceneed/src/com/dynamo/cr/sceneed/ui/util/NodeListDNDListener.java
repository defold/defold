package com.dynamo.cr.sceneed.ui.util;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerDropAdapter;
import org.eclipse.swt.dnd.DND;
import org.eclipse.swt.dnd.DragSourceEvent;
import org.eclipse.swt.dnd.DragSourceListener;
import org.eclipse.swt.dnd.DropTargetEvent;
import org.eclipse.swt.dnd.TransferData;

import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;

public class NodeListDNDListener extends ViewerDropAdapter implements DragSourceListener {

    private final ISceneView.IPresenter presenter;
    private final ISceneView.IPresenterContext presenterContext;

    public NodeListDNDListener(Viewer viewer, ISceneView.IPresenter presenter, ISceneView.IPresenterContext presenterContext) {
        super(viewer);
        this.presenter = presenter;
        this.presenterContext = presenterContext;
        // Removes visual feedback, like horizontal lines, between items when using DND
        // Should be removed once we support custom ordering
        setFeedbackEnabled(false);
    }

    @Override
    public void dragStart(DragSourceEvent event) {
        List<Node> nodes = getSelectedNodes();
        if (nodes.isEmpty()) {
            event.doit = false;
            return;
        }
        // check siblings
        Node parent = nodes.get(0).getParent();
        if (parent == null) {
            event.doit = false;
            return;
        }
        for (Node node : nodes) {
            if (parent != node.getParent()) {
                event.doit = false;
                return;
            }
        }
    }

    @Override
    public void dragSetData(DragSourceEvent event) {
        event.data = getSelectedNodes();
    }

    @Override
    public void dragFinished(DragSourceEvent event) {
        // handled in performDrop below
    }

    @SuppressWarnings("unchecked")
    @Override
    public boolean performDrop(Object data) {
        switch (getCurrentOperation()) {
        case DND.DROP_MOVE:
            presenter.onDNDMoveSelection(presenterContext, (List<Node>)data, (Node)getCurrentTarget());
            break;
        case DND.DROP_COPY:
            presenter.onDNDDuplicateSelection(presenterContext, (List<Node>)data, (Node)getCurrentTarget());
            break;
        }
        return false;
    }

    @Override
    public boolean validateDrop(Object target, int operation, TransferData transferType) {
        if (target == null) {
            return false;
        }
        Node targetNode = (Node)target;
        List<Node> nodes = getSelectedNodes();
        if (nodes.contains(targetNode)) {
            return false;
        }
        Node parent = nodes.get(0).getParent();
        if (operation == DND.DROP_MOVE && parent == targetNode) {
            return false;
        }
        return true;
    }

    @Override
    protected Object determineTarget(DropTargetEvent event) {
        if (event.item == null) {
            return null;
        }
        Node target = (Node)event.item.getData();
        List<Node> nodes = getSelectedNodes();
        return NodeUtil.findDropTarget(target, nodes, this.presenterContext);
    }

    private List<Node> getSelectedNodes() {
        IStructuredSelection selection = (IStructuredSelection)getViewer().getSelection();
        int n = selection.size();
        if (n == 0) {
            return new ArrayList<Node>();
        }
        List<Node> nodes = new ArrayList<Node>(n);
        for (Object obj : selection.toList()) {
            nodes.add((Node)obj);
        }
        return nodes;
    }

}
