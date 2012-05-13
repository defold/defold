package com.dynamo.cr.sceneed.core;

import com.dynamo.cr.sceneed.ui.RootManipulator;


public interface IManipulatorRegistry {

    RootManipulator getManipulatorForSelection(IManipulatorMode mode, Object[] selection);

    IManipulatorMode getMode(String modeId);

}
