package com.dynamo.cr.sceneed.core;

public interface IManipulatorFactory {

    boolean match(Object[] selection);

    IManipulator create();

}
