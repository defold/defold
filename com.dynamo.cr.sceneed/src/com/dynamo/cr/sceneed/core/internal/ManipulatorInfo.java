package com.dynamo.cr.sceneed.core.internal;

import com.dynamo.cr.sceneed.core.IManipulatorInfo;
import com.dynamo.cr.sceneed.core.IManipulatorMode;

public class ManipulatorInfo implements IManipulatorInfo {

    private String name;
    private IManipulatorMode mode;
    private String nodeType;

    public ManipulatorInfo(String name, IManipulatorMode mode, String nodeType) {
        this.name = name;
        this.mode = mode;
        this.nodeType = nodeType;
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
    public String getNodeType() {
        return nodeType;
    }

}
