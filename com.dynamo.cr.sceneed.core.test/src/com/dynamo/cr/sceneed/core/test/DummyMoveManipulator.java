package com.dynamo.cr.sceneed.core.test;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Node;

public class DummyMoveManipulator extends AbstractTestManipulator {

    @Override
    public boolean match(Object[] selection) {
        for (Object object : selection) {
            if (object instanceof Node) {
                return true;
            }
        }
        return false;
    }

    @Override
    public void mouseUp(MouseEvent e) {
        DummyMoveOperation operation = new DummyMoveOperation();
        this.controller.executeOperation(operation);
    }

}
