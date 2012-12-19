package com.dynamo.cr.sceneed.ui.util;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerDropAdapter;
import org.eclipse.swt.dnd.DND;
import org.eclipse.swt.dnd.DragSourceEvent;
import org.eclipse.swt.dnd.DragSourceListener;
import org.eclipse.swt.dnd.TransferData;

import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;

public class NodeListDNDListener extends ViewerDropAdapter implements DragSourceListener {

    private static class DropTarget {
        public DropTarget(Node node, int index) {
            this.node = node;
            this.index = index;
        }

        public Node node;
        public int index;
    }

    private final ISceneView.IPresenter presenter;
    private final ISceneView.IPresenterContext presenterContext;
    private boolean supportsReordering = false;

    public NodeListDNDListener(Viewer viewer, ISceneView.IPresenter presenter, ISceneView.IPresenterContext presenterContext, boolean supportsReordering) {
        super(viewer);
        this.presenter = presenter;
        this.presenterContext = presenterContext;
        this.supportsReordering = supportsReordering;
        setFeedbackEnabled(supportsReordering);
    }

    public void setSupportsReordering(boolean supportsReordering) {
        this.supportsReordering = supportsReordering;
        setFeedbackEnabled(supportsReordering);
    }

    @Override
    public void dragStart(DragSourceEvent event) {
        List<Node> nodes = getSelectedNodes();
        if (nodes.isEmpty()) {
            event.doit = false;
            return;
        }
        // Only allow siblings to be dragged
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

    private DropTarget getDropTarget(Object target, int location) {
        if (!(target instanceof Node)) {
            return null;
        }
        Node targetNode = (Node)target;
        int index = targetNode.getChildren().size();
        if (supportsReordering && (location == LOCATION_AFTER || location == LOCATION_BEFORE)) {
            if (targetNode.getParent() == null) {
                return null;
            }
            index = targetNode.getParent().getChildren().indexOf(target);
            // If we don't get a valid index, there is a dangling child which is critical
            assert(index >= 0);
            if (location == LOCATION_AFTER) {
                ++index;
            }
            targetNode = targetNode.getParent();
        }
        return new DropTarget(targetNode, index);
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
        DropTarget dropTarget = getDropTarget(getCurrentTarget(), getCurrentLocation());
        if (dropTarget == null) {
            return false;
        }
        switch (getCurrentOperation()) {
        case DND.DROP_MOVE:
            presenter.onDNDMoveSelection(presenterContext, (List<Node>)data, dropTarget.node, dropTarget.index);
            return true;
        case DND.DROP_COPY:
            presenter.onDNDDuplicateSelection(presenterContext, (List<Node>)data, dropTarget.node, dropTarget.index);
            return true;
        default:
            return false;
        }
    }

    @Override
    public boolean validateDrop(Object target, int operation, TransferData transferType) {
        DropTarget dropTarget = getDropTarget(target, getCurrentLocation());
        if (dropTarget == null) {
            return false;
        }
        List<Node> nodes = getSelectedNodes();
        if (nodes.contains(dropTarget.node)) {
            return false;
        }
        return NodeUtil.findValidAncestor(dropTarget.node, nodes, this.presenterContext) == dropTarget.node;
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
