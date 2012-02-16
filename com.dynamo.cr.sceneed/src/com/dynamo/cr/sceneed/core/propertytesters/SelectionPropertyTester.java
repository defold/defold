package com.dynamo.cr.sceneed.core.propertytesters;

import org.eclipse.core.expressions.PropertyTester;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.Node;

public class SelectionPropertyTester extends PropertyTester {

    public SelectionPropertyTester() {
    }

    @Override
    public boolean test(Object receiver, String property, Object[] args, Object expectedValue) {
        if (receiver instanceof IStructuredSelection) {
            IStructuredSelection selection = (IStructuredSelection)receiver;
            if (property.equals("siblings")) {
                Node parent = null;
                for (Object object : selection.toArray()) {
                    if (!(object instanceof Node)) {
                        return false;
                    }
                    Node node = (Node)object;
                    if (parent == null) {
                        parent = node.getParent();
                        if (parent == null) {
                            return false;
                        }
                    } else if (parent != node.getParent()) {
                        return false;
                    }
                }
                return parent != null;
            }
        }
        return false;
    }

}
