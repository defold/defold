package com.dynamo.cr.sceneed.ui;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class SelectManipulator extends RootManipulator {

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
    protected void transformChanged() {
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
    }

    @Override
    protected void selectionChanged() {
    }

    @Override
    public void refresh() {
    }

}
