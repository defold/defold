package com.dynamo.cr.sceneed.core.test;

import com.dynamo.cr.sceneed.core.Manipulator;

public class DummySphereSizeManipulator extends Manipulator {

    @Override
    public boolean match(Object[] selection) {
        for (Object object : selection) {
            if (object instanceof DummySphere) {
                return true;
            }
        }
        return false;
    }

}
