package com.dynamo.cr.goprot;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.StructuredSelection;

public class NodeSelectionProvider implements ISelectionProvider {

    private final List<ISelectionChangedListener> selectionListeners = new ArrayList<ISelectionChangedListener>();
    private IStructuredSelection selection = new StructuredSelection();

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
    }

}
