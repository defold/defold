package com.dynamo.cr.go.core;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public abstract class GameObjectInstanceNode extends InstanceNode {

    public abstract void sortChildren();

    @Override
    public void childAdded(Node child) {
        sortChildren();
    }

    @Override
    protected boolean isValidParent(Node node) {
        while(node != null) {
            if(node instanceof CollectionNode) {
                return true;
            }
            node = node.getParent();
        }
        return false;
    }

    @Override
    protected void childRemoved(Node child) {
        // TODO this is a poor way to remove children from the selection
        // It should preferably be done in Node immediately, but seemed like a too complex change at the moment
        if (getModel() != null) {
            IStructuredSelection selection = getModel().getSelection();
            if (selection != null) {
                Object[] objects = selection.toArray();
                List<Object> list = new ArrayList<Object>(objects.length);
                for (Object o : objects) {
                    if (o != child) {
                        list.add(o);
                    }
                }
                if (objects.length != list.size()) {
                    getModel().setSelection(new StructuredSelection(list));
                }
            }
        }
    }

}
