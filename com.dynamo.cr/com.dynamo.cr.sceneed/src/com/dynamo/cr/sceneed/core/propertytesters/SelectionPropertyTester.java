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
                Object[] objects = selection.toArray();
                for (Object object : objects) {
                    if (!(object instanceof Node)) {
                        return false;
                    }
                    Node node = (Node)object;
                    if (parent == null) {
                        parent = node.getParent();
                        if (parent == null && objects.length == 1) {
                            return true;
                        }
                    } else if (parent != node.getParent()) {
                        return false;
                    }
                }
                if (expectedValue != null) {
                    expectedValue.equals(parent != null);
                }
                return parent != null;
            } else if (property.equals("root")) {
                Object[] objects = selection.toArray();
                boolean isRoot = objects.length == 1 && objects[0] instanceof Node && ((Node)objects[0]).getParent() == null;
                if (expectedValue != null) {
                    return expectedValue.equals(isRoot);
                }
                return isRoot;
            }
        }
        return false;
    }

}
