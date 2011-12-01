package com.dynamo.cr.sceneed.core.test;

import com.dynamo.cr.sceneed.core.IManipulator;
import com.dynamo.cr.sceneed.core.IManipulatorFactory;

public class DummySphereSizeManipulatorFactory implements IManipulatorFactory {

    public DummySphereSizeManipulatorFactory() {
    }

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
    public IManipulator create() {
        return new DummySphereSizeManipulator();
    }

}
