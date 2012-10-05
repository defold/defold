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
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewController;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;

public class SceneRenderViewProvider implements IRenderViewProvider, ISelectionProvider, IRenderViewController {

    private IRenderView renderView;
    private IPresenterContext presenterContext;
    private Node root;

    // SelectionProvider
    private final List<ISelectionChangedListener> selectionListeners = new ArrayList<ISelectionChangedListener>();
    private IStructuredSelection selection = new StructuredSelection();

    // Scene selections
    private SelectionBoxNode selectionBoxNode;
    private boolean selecting = false;
    private boolean dragSelect = false;
    private static final int DRAG_MARGIN = 2;
    private List<Node> originalSelection = Collections.emptyList();

    @Inject
    public SceneRenderViewProvider(IRenderView renderView, IPresenterContext presenterContext) {
        this.renderView = renderView;
        this.presenterContext = presenterContext;
        this.selectionBoxNode = new SelectionBoxNode();
        renderView.addRenderProvider(this);
        renderView.addRenderController(this);
    }

    public void setRoot(Node root) {
        this.root = root;
    }

    @PreDestroy
    public void dispose() {
        renderView.removeRenderProvider(this);
        renderView.removeRenderController(this);
    }

    @Override
    public void setup(RenderContext renderContext) {
        if (root != null) {
            renderView.setupNode(renderContext, root);
        }
        if (renderContext.getPass().equals(Pass.OVERLAY) && this.selectionBoxNode.isVisible()) {
            this.renderView.setupNode(renderContext, this.selectionBoxNode);
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
        if (!this.selecting && this.selection != selection && selection instanceof IStructuredSelection) {
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
    public FocusType getFocusType(List<Node> nodes, MouseEvent event) {
        if (event.button == 1) {
            return FocusType.SELECTION;
        } else {
            return FocusType.NONE;
        }
    }

    @Override
    public void initControl(List<Node> nodes) {
        this.originalSelection = new ArrayList<Node>(this.selection.size());
        for (Object o : this.selection.toList()) {
            this.originalSelection.add((Node) o);
        }
        this.selecting = true;
    }

    @Override
    public void finalControl() {
        this.selectionBoxNode.setVisible(false);
        this.selecting = false;
        this.dragSelect = false;
        this.originalSelection.clear();
        this.presenterContext.setSelection(this.selection);
        this.presenterContext.refreshView();
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
    }

    @Override
    public void mouseDown(MouseEvent e) {
        this.selectionBoxNode.set(e.x, e.y);
        this.dragSelect = false;
        IStructuredSelection selection = boxSelect(e);
        this.presenterContext.setSelection(selection);
        this.presenterContext.refreshView();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        IStructuredSelection selection = boxSelect(e);
        this.selecting = false;
        setSelection(selection);
        fireSelectionChanged(selection);
    }

    @Override
    public void mouseMove(MouseEvent e) {
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
        IStructuredSelection selection = boxSelect(e);
        this.presenterContext.setSelection(selection);
        this.presenterContext.refreshView();
    }

    private IStructuredSelection boxSelect(MouseEvent event) {
        this.selectionBoxNode.setCurrent(event.x, event.y);
        return boxSelect(event.stateMask);
    }

    private IStructuredSelection boxSelect(int stateMask) {
        List<Node> nodes = this.renderView.findNodesBySelection(this.selectionBoxNode.getStart(), this.selectionBoxNode.getCurrent());
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
        boolean macModifiers = (stateMask & (SWT.MOD1 | SWT.SHIFT)) != 0;
        boolean othersModifiers = (stateMask & SWT.CTRL) != 0;
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
        return new StructuredSelection(selected);
    }

    @Override
    public void keyPressed(KeyEvent e) {

    }

    @Override
    public void keyReleased(KeyEvent e) {

    }
}
