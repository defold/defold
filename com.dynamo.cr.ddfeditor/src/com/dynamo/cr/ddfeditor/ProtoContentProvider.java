package com.dynamo.cr.ddfeditor;

import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.Viewer;

import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.cr.protobind.Node;

public class ProtoContentProvider implements ITreeContentProvider {

    private MessageNode message;

    @Override
    public void dispose() {}

    @Override
    public void inputChanged(Viewer viewer, Object oldInput, Object newInput) {
        this.message = (MessageNode) newInput;
    }

    @Override
    public Object[] getElements(Object inputElement) {
        if (inputElement instanceof MessageNode) {
            MessageNode message = (MessageNode) inputElement;
            return message.getAllPaths();
        }
        return null;
    }

    @Override
    public Object[] getChildren(Object parentElement) {
        if (parentElement instanceof IPath) {
            IPath fieldPath = (IPath) parentElement;
            Object value = message.getField(fieldPath);
            if (value instanceof Node) {
                Node node = (Node) value;
                return node.getAllPaths();
            }
        }
        return null;
    }

    @Override
    public Object getParent(Object element) {
        return null;
    }

    @Override
    public boolean hasChildren(Object element) {
        if (element instanceof IPath) {
            IPath fieldPath = (IPath) element;
            Object value = message.getField(fieldPath);
            if (value instanceof Node) {
                Node node = (Node) value;
                return node.getAllPaths().length > 0;
            }
        }
        return false;
    }
}
