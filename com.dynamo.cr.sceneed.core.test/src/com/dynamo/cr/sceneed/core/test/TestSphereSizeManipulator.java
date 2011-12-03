package com.dynamo.cr.sceneed.core.test;

public class TestSphereSizeManipulator extends AbstractTestManipulator {

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
