package com.dynamo.cr.sceneed.core;


public interface IManipulatorRegistry {

    IManipulator getManipulatorForSelection(IManipulatorMode mode, Object[] selection);

    IManipulatorMode getMode(String modeId);

}
