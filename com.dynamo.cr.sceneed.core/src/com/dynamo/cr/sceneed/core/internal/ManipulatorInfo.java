package com.dynamo.cr.sceneed.core.internal;

import com.dynamo.cr.sceneed.core.IManipulatorFactory;
import com.dynamo.cr.sceneed.core.IManipulatorInfo;
import com.dynamo.cr.sceneed.core.IManipulatorMode;

public class ManipulatorInfo implements IManipulatorInfo {

    private String name;
    private IManipulatorMode mode;
    private IManipulatorFactory factory;

    public ManipulatorInfo(String name, IManipulatorMode mode, IManipulatorFactory factory) {
        this.name = name;
        this.mode = mode;
        this.factory = factory;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public IManipulatorMode getMode() {
        return mode;
    }

    @Override
    public IManipulatorFactory getFactory() {
        return factory;
    }

}
