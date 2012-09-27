package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.annotation.PreDestroy;
import javax.inject.Inject;
import javax.vecmath.Point2i;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;

public class SceneRenderViewProvider implements IRenderViewProvider, ISelectionProvider, MouseMoveListener,
        MouseListener {

    private IRenderView renderView;
    private IPresenterContext presenterContext;
    private Node root;

    // SelectionProvider
    private final List<ISelectionChangedListener> selectionListeners = new ArrayList<ISelectionChangedListener>();
    private IStructuredSelection selection = new StructuredSelection();

    // Scene selections
    private boolean selecting = false;
    private SelectionBoxNode selectionBoxNode;
    private boolean dragSelect = false;
    private static final int DRAG_MARGIN = 2;
    private List<Node> originalSelection = Collections.emptyList();

    @Inject
    public SceneRenderViewProvider(IRenderView renderView, IPresenterContext presenterContext) {
        this.renderView = renderView;
        this.presenterContext = presenterContext;
        this.selectionBoxNode = new SelectionBoxNode();
        renderView.addMouseListener(this);
        renderView.addMouseMoveListener(this);
        renderView.addRenderProvider(this);
    }

    public void setRoot(Node root) {
        this.root = root;
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
        if (renderContext.getPass().equals(Pass.OVERLAY)) {
            this.renderView.setupNode(renderContext, this.selectionBoxNode);
        }
    }

    @Override
    public void onNodeHit(List<Node> nodes, MouseEvent event, MouseEventType mouseEventType) {
        this.selecting = true;
        // Save the orginial selection for later use when multi/toggle-selecting
        if (mouseEventType == MouseEventType.MOUSE_DOWN) {
            this.originalSelection = new ArrayList<Node>(this.selection.size());
            for (Object o : this.selection.toList()) {
                this.originalSelection.add((Node) o);
            }
        }
        // TODO This should probably be filtered differently
        List<Node> filteredNodes = new ArrayList<Node>(nodes.size());
        for (Node n : nodes) {
            if (!(n instanceof Manipulator)) {
                filteredNodes.add(n);
            }
        }
        // If single click, only select one node
        if (!this.dragSelect && !filteredNodes.isEmpty()) {
            filteredNodes = Collections.singletonList(filteredNodes.get(0));
        }
        // Only add editable nodes or their editable ancestors
        Set<Node> selectedNodes = new HashSet<Node>();
        for (Node node : filteredNodes) {
            Node selectee = null;
            if (node.isEditable()) {
                selectee = node;
            } else {
                Node parent = node.getParent();
                while (parent != null && !parent.isEditable()) {
                    parent = parent.getParent();
                }
                if (parent != null) {
                    selectee = parent;
                }
            }
            if (selectee != null) {
                selectedNodes.add(selectee);
            }
        }
        // Handle multi-select and toggling
        boolean macModifiers = (event.stateMask & (SWT.MOD1 | SWT.SHIFT)) != 0;
        boolean othersModifiers = (event.stateMask & SWT.CTRL) != 0;
        boolean multiSelect = macModifiers || (!EditorUtil.isMac() && othersModifiers);
        if (multiSelect) {
            for (Node node : this.originalSelection) {
                if (selectedNodes.contains(node)) {
                    selectedNodes.remove(node);
                } else {
                    selectedNodes.add(node);
                }
            }
        }
        // Make sure the root is selected at empty selections
        if (selectedNodes.isEmpty()) {
            selectedNodes.add(this.root);
        }
        List<Node> selected = new ArrayList<Node>(selectedNodes);
        StructuredSelection newSelection = new StructuredSelection(selected);
        // Finalize selection with operation if mouse up, pure visualization
        // otherwise
        if (mouseEventType == MouseEventType.MOUSE_UP) {
            fireSelectionChanged(newSelection);
            this.selecting = false;
            setSelection(newSelection);
        } else {
            this.presenterContext.setSelection(newSelection);
            this.presenterContext.refreshView();
        }
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
        if (this.selection != selection && selection instanceof IStructuredSelection) {
            this.selection = (IStructuredSelection)selection;
        }
    }

    private void fireSelectionChanged(ISelection selection) {
        SelectionChangedEvent event = new SelectionChangedEvent(this, this.selection);
        for (ISelectionChangedListener listener : this.selectionListeners) {
            listener.selectionChanged(event);
        }
    }

    @Override
    public boolean hasFocus(List<Node> nodes) {
        return this.selecting;
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
    }

    @Override
    public void mouseDown(MouseEvent e) {
        if (this.selecting) {
            this.selectionBoxNode.set(e.x, e.y);
            this.dragSelect = false;
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        this.selectionBoxNode.setVisible(false);
        this.dragSelect = false;
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (this.selecting) {
            this.selectionBoxNode.setCurrent(e.x, e.y);
            if (!this.dragSelect) {
                // Check if the mouse has moved far enough to be considered
                // dragging
                Point2i delta = new Point2i(this.selectionBoxNode.getCurrent());
                delta.sub(this.selectionBoxNode.getStart());
                delta.absolute();
                if (delta.x > DRAG_MARGIN || delta.y > DRAG_MARGIN) {
                    this.dragSelect = true;
                    this.selectionBoxNode.setVisible(true);
                }
            }
        }
    }
}
