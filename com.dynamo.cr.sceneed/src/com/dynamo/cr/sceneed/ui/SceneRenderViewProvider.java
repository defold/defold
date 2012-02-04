package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import javax.annotation.PreDestroy;
import javax.inject.Inject;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;

public class SceneRenderViewProvider implements IRenderViewProvider, ISelectionProvider {

    private IRenderView renderView;
    private Node root;

    // SelectionProvider
    private final List<ISelectionChangedListener> selectionListeners = new ArrayList<ISelectionChangedListener>();
    private IStructuredSelection selection = new StructuredSelection();

    @Inject
    public SceneRenderViewProvider(IRenderView renderView) {
        this.renderView = renderView;
        renderView.addRenderProvider(this);
    }

    public void setRoot(Node root) {
        this.root = root;
        renderView.refresh();
    }

    @PreDestroy
    public void dispose() {
        renderView.removeRenderProvider(this);
    }

    @Override
    public void setup(RenderContext renderContext) {
        if (root != null) {
            renderView.setupNode(renderContext, root);
        }
    }

    @Override
    public void onNodeHit(List<Node> nodes) {
        // TODO: We should test this behavior...

        for (Node node : nodes) {
            if (node instanceof Manipulator) {
                // Do nothing when a manipulator is selected,
                // ie do not changed current selection
                return;
            }
        }
        List<Node> selected = new ArrayList<Node>(nodes.size());
        for (Node node : nodes) {
            if (node.isEditable()) {
                selected.add(node);
            } else {
                Node parent = node.getParent();
                while (parent != null && !parent.isEditable()) {
                    parent = parent.getParent();
                }
                if (parent != null) {
                    selected.add(parent);
                }
            }
        }
        StructuredSelection newSelection = new StructuredSelection(selected);
        setSelection(newSelection);
    }

    // SelectionProvider

    @Override
    public void addSelectionChangedListener(ISelectionChangedListener listener) {
        this.selectionListeners.add(listener);
    }

    @Override
    public ISelection getSelection() {
        return this.selection;
    }

    @Override
    public void removeSelectionChangedListener(
            ISelectionChangedListener listener) {
        this.selectionListeners.remove(listener);
    }

    @Override
    public void setSelection(ISelection selection) {
        if (selection instanceof IStructuredSelection) {
            this.selection = (IStructuredSelection)selection;
            SelectionChangedEvent event = new SelectionChangedEvent(this, this.selection);
            for (ISelectionChangedListener listener : this.selectionListeners) {
                listener.selectionChanged(event);
            }
        }
        renderView.refresh();
    }

}
