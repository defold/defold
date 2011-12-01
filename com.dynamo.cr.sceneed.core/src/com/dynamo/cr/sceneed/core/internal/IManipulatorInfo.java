package com.dynamo.cr.sceneed.core.internal;

import com.dynamo.cr.sceneed.core.IManipulatorFactory;
import com.dynamo.cr.sceneed.core.IManipulatorMode;

public interface IManipulatorInfo {

    IManipulatorFactory getFactory();

    IManipulatorMode getMode();

    String getName();

}