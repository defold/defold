package com.dynamo.cr.guieditor;

import java.awt.geom.Rectangle2D;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.guieditor.scene.GuiNode;

public class GuiSelectionProvider implements ISelectionProvider {
    private List<ISelectionChangedListener> listeners = new ArrayList<ISelectionChangedListener>();

    private ArrayList<GuiNode> nodes = new ArrayList<GuiNode>();
    private Rectangle2D.Double selectionBounds = new Rectangle2D.Double(0, 0, 0, 0);

    public void setSelection(List<GuiNode> selection) {
        // Make sure to create a new list here. The list is returned in getSelectionList()
        nodes = new ArrayList<GuiNode>(selection.size());
        nodes.addAll(selection);

        SelectionChangedEvent event = new SelectionChangedEvent(GuiSelectionProvider.this, new StructuredSelection(nodes));
        for (ISelectionChangedListener listener : listeners) {
            listener.selectionChanged(event);
        }
    }

    public void setSelectionNoFireEvent(List<GuiNode> selection) {
        // Make sure to create a new list here. The list is returned in getSelectionList()
        nodes = new ArrayList<GuiNode>();
        nodes.addAll(selection);
    }

    public List<GuiNode> getSelectionList() {
        return Collections.unmodifiableList(nodes);
    }

    public Rectangle2D.Double getBounds() {
        double xmin = Double.MAX_VALUE;
        double ymin = Double.MAX_VALUE;
        double xmax = -Double.MAX_VALUE;
        double ymax = -Double.MAX_VALUE;
        for (GuiNode node : nodes) {
            Rectangle2D nodeBounds = node.getVisualBounds();

            xmin = Math.min(xmin, nodeBounds.getX());
            ymin = Math.min(ymin, nodeBounds.getY());
            xmax = Math.max(xmax, nodeBounds.getX() + nodeBounds.getWidth());
            ymax = Math.max(ymax, nodeBounds.getY() + nodeBounds.getHeight());
        }

        selectionBounds = new Rectangle2D.Double(xmin, ymin, xmax-xmin, ymax-ymin);
        return new Rectangle2D.Double(selectionBounds.x, selectionBounds.y, selectionBounds.width, selectionBounds.height);
    }

    public int size() {
        return nodes.size();
    }

    @Override
    public void addSelectionChangedListener(ISelectionChangedListener listener) {
        if (!this.listeners.contains(listener))
            this.listeners.add(listener);
    }

    @Override
    public void removeSelectionChangedListener(ISelectionChangedListener listener) {
        this.listeners.remove(listener);
    }

    @Override
    public void setSelection(ISelection selection) {
    }

    @Override
    public ISelection getSelection() {
        return new StructuredSelection(nodes);
    }

}
