package com.dynamo.cr.sceneed.core.test;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Node;

public class TestMoveManipulator extends AbstractTestManipulator {

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
        TestMoveOperation operation = new TestMoveOperation();
        this.controller.executeOperation(operation);
    }

}
