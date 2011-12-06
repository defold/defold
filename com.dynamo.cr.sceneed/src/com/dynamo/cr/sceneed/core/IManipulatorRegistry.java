package com.dynamo.cr.sceneed.core;


public interface IManipulatorRegistry {

    Manipulator getManipulatorForSelection(IManipulatorMode mode, Object[] selection);

    IManipulatorMode getMode(String modeId);

}
