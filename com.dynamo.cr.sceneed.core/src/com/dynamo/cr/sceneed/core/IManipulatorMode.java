package com.dynamo.cr.sceneed.core;

import java.util.List;

import com.dynamo.cr.sceneed.core.internal.IManipulatorInfo;

public interface IManipulatorMode {

    public abstract String getId();

    public abstract String getName();

    public abstract List<IManipulatorInfo> getManipulatorInfoList();

}