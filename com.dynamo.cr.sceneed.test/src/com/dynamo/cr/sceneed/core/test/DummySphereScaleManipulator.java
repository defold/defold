package com.dynamo.cr.sceneed.core.test;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.ui.RootManipulator;

public class DummySphereScaleManipulator extends RootManipulator {

    @Override
    public boolean match(Object[] selection) {
        for (Object object : selection) {
            if (object instanceof DummySphere) {
                return true;
            }
        }
        return false;
    }

    @Override
    protected void transformChanged() {
    }

    @Override
    protected void selectionChanged() {
    }

    @Override
    public void refresh() {
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
    }

}
